//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <pak-io/pak-writer.hxx>

namespace test
{
    namespace
    {
        auto makePayload(std::size_t size, std::uint8_t seed) -> std::vector<std::uint8_t>
        {
            std::vector<std::uint8_t> payload(size);
            for (std::size_t index = 0; index < payload.size(); ++index)
            {
                payload[index] = static_cast<std::uint8_t>((index * 31u + seed) % 251u);
            }
            return payload;
        }

        void writeFile(std::filesystem::path const& path, std::vector<std::uint8_t> const& payload)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            REQUIRE(file.good());
            file.write(reinterpret_cast<char const*>(payload.data()), static_cast<std::streamsize>(payload.size()));
            file.close();
        }

        // Read back with a plain stream so the writer tests do not depend on the reader.
        auto readFile(std::filesystem::path const& path) -> std::vector<std::uint8_t>
        {
            std::ifstream file(path, std::ios::binary);
            REQUIRE(file.good());
            return std::vector<std::uint8_t>(std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{});
        }

        auto readOffset(std::vector<std::uint8_t> const& bytes, std::size_t offset) -> std::uint64_t
        {
            std::uint64_t value{};
            REQUIRE(offset + sizeof(value) <= bytes.size());
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
    } // namespace

    TEST_CASE("pak.writer.Layout")
    {
        auto const directory = std::filesystem::temp_directory_path() / "pak-io-test-writer-layout";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        auto const alpha = makePayload(11u, 0u);
        auto const beta = makePayload(64u * 1024u + 17u, 7u);
        writeFile(directory / "alpha.bin", alpha);
        writeFile(directory / "beta.bin", beta);

        auto const    path = directory / "archive.pak";
        pk::PakWriter writer;
        REQUIRE(writer.process(path, { directory / "alpha.bin", directory / "beta.bin" }));

        auto const bytes = readFile(path);
        REQUIRE(bytes.size() >= 32u);
        CHECK(std::memcmp(bytes.data(), "PAKFILE", 8u) == 0);
        CHECK(readOffset(bytes, 16u) == 2u);

        // Header, then two 32-byte entries.
        auto const alphaSize = readOffset(bytes, 40u);
        auto const alphaData = readOffset(bytes, 48u);
        auto const betaSize = readOffset(bytes, 72u);
        auto const betaData = readOffset(bytes, 80u);

        CHECK(alphaSize == alpha.size());
        CHECK(betaSize == beta.size());
        CHECK(alphaData % 32u == 0u);
        CHECK(betaData % 32u == 0u);
        CHECK(betaData >= alphaData + alphaSize);

        REQUIRE(alphaData + alphaSize <= bytes.size());
        CHECK(std::equal(alpha.begin(), alpha.end(), bytes.begin() + static_cast<std::ptrdiff_t>(alphaData)));

        REQUIRE(betaData + betaSize <= bytes.size());
        CHECK(std::equal(beta.begin(), beta.end(), bytes.begin() + static_cast<std::ptrdiff_t>(betaData)));

        std::filesystem::remove_all(directory);
    }

    TEST_CASE("pak.writer.MissingSourceFile")
    {
        auto const directory = std::filesystem::temp_directory_path() / "pak-io-test-writer-missing";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        auto const    path = directory / "archive.pak";
        pk::PakWriter writer;
        CHECK_FALSE(writer.process(path, { directory / "does-not-exist.bin" }));
        CHECK_FALSE(std::filesystem::exists(path));

        std::filesystem::remove_all(directory);
    }
} // namespace test
