//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pak-io/pak-reader.hxx>
#include <pak-io/pak-writer.hxx>

namespace test
{
    namespace
    {
        auto makePayload(std::size_t size, std::uint8_t seed = 0u) -> std::vector<std::uint8_t>
        {
            std::vector<std::uint8_t> payload(size);
            for (std::size_t index = 0; index < payload.size(); ++index)
            {
                payload[index] = static_cast<std::uint8_t>((index * 31u + seed) % 251u);
            }
            return payload;
        }

        // Written with a plain stream so the source tests fail independently of the sink.
        void writeFile(std::filesystem::path const& path, std::vector<std::uint8_t> const& payload)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            REQUIRE(file.good());
            file.write(reinterpret_cast<char const*>(payload.data()), static_cast<std::streamsize>(payload.size()));
            file.close();
        }

        /// Creates a scratch directory, unique to the test, holding the given source files.
        auto makeDirectory(std::string_view const name) -> std::filesystem::path
        {
            auto const directory = std::filesystem::temp_directory_path() / name;
            std::filesystem::remove_all(directory);
            std::filesystem::create_directories(directory);
            return directory;
        }
    } // namespace

    TEST_CASE("pak.reader.RoundTrip")
    {
        auto const directory = makeDirectory("pak-io-test-round-trip");

        auto const alpha = makePayload(11u, 0u);
        auto const beta = makePayload(64u * 1024u + 17u, 7u);
        writeFile(directory / "alpha.bin", alpha);
        writeFile(directory / "beta.bin", beta);

        auto const    path = directory / "archive.pak";
        pk::PakWriter writer;
        REQUIRE(writer.process(path, { directory / "alpha.bin", directory / "beta.bin" }));

        pk::PakReader reader;
        REQUIRE(reader.open(path));
        CHECK(reader.name() == "archive");

        auto const entries = reader.entries();
        REQUIRE(entries.size() == 2u);
        CHECK(entries[0].identifier == "alpha");
        CHECK(entries[1].identifier == "beta");

        REQUIRE(entries[0].size == alpha.size());
        CHECK(std::equal(entries[0].data, entries[0].data + entries[0].size, alpha.begin()));

        REQUIRE(entries[1].size == beta.size());
        CHECK(std::equal(entries[1].data, entries[1].data + entries[1].size, beta.begin()));

        reader.close();
        std::filesystem::remove_all(directory);
    }

    TEST_CASE("pak.reader.Find")
    {
        auto const directory = makeDirectory("pak-io-test-find");

        auto const payload = makePayload(4096u, 3u);
        writeFile(directory / "gamma.bin", payload);

        auto const    path = directory / "archive.pak";
        pk::PakWriter writer;
        REQUIRE(writer.process(path, { directory / "gamma.bin" }));

        pk::PakReader reader;
        REQUIRE(reader.open(path));

        auto const* const entry = reader.find("gamma");
        REQUIRE(entry != nullptr);
        REQUIRE(entry->size == payload.size());
        CHECK(std::equal(entry->data, entry->data + entry->size, payload.begin()));

        CHECK(reader.find("missing") == nullptr);

        reader.close();
        std::filesystem::remove_all(directory);
    }

    TEST_CASE("pak.reader.Empty")
    {
        auto const directory = makeDirectory("pak-io-test-empty");

        auto const    path = directory / "empty.pak";
        pk::PakWriter writer;
        REQUIRE(writer.process(path, {}));

        pk::PakReader reader;
        REQUIRE(reader.open(path));
        CHECK(reader.name() == "empty");
        CHECK(reader.entries().empty());
        CHECK(reader.find("anything") == nullptr);

        reader.close();
        std::filesystem::remove_all(directory);
    }

    TEST_CASE("pak.reader.RejectsNonPakFiles")
    {
        auto const path = std::filesystem::temp_directory_path() / "pak-io-test-not-a-pak.pak";
        writeFile(path, makePayload(1024u));

        pk::PakReader reader;
        CHECK_FALSE(reader.open(path));

        std::filesystem::remove(path);
    }

    TEST_CASE("pak.reader.RejectsTruncatedFiles")
    {
        auto const directory = makeDirectory("pak-io-test-truncated");

        writeFile(directory / "delta.bin", makePayload(2048u, 5u));

        auto const    path = directory / "archive.pak";
        pk::PakWriter writer;
        REQUIRE(writer.process(path, { directory / "delta.bin" }));

        // Lop off the payload; the entry table still claims it is there.
        std::filesystem::resize_file(path, 96u);

        pk::PakReader reader;
        CHECK_FALSE(reader.open(path));

        std::filesystem::remove_all(directory);
    }

    TEST_CASE("pak.source.RandomAccess")
    {
        auto const directory = makeDirectory("pak-io-test-random");

        writeFile(directory / "epsilon.bin", makePayload(4096u, 9u));

        auto const    path = directory / "archive.pak";
        pk::PakWriter writer;
        REQUIRE(writer.process(path, { directory / "epsilon.bin" }));

        pk::PakReader reader;
        REQUIRE(reader.open(path));
        REQUIRE(reader.view().size() == reader.size());
        CHECK(reader.position() == 0u);

        std::vector<std::uint8_t> magic(7u);
        reader.read(magic);
        CHECK(std::string(magic.begin(), magic.end()) == "PAKFILE");
        CHECK(reader.position() == magic.size());

        std::vector<std::uint8_t> tail(16u);
        reader.readAt(tail, reader.size() - tail.size());
        CHECK(reader.position() == magic.size());

        reader.seek(0u);
        CHECK(reader.position() == 0u);

        reader.close();
        std::filesystem::remove_all(directory);
    }

    TEST_CASE("pak.source.ReadOutOfRange")
    {
        auto const directory = makeDirectory("pak-io-test-range");

        writeFile(directory / "zeta.bin", makePayload(128u, 11u));

        auto const    path = directory / "archive.pak";
        pk::PakWriter writer;
        REQUIRE(writer.process(path, { directory / "zeta.bin" }));

        pk::PakReader reader;
        REQUIRE(reader.open(path));
        auto const size = reader.size();

        std::vector<std::uint8_t> buffer(8u);
        CHECK_THROWS_AS(reader.readAt(buffer, size), std::out_of_range);
        CHECK_THROWS_AS(reader.readAt(buffer, size - 4u), std::out_of_range);
        CHECK_THROWS_AS(reader.seek(size + 1u), std::out_of_range);

        reader.seek(size);
        CHECK_THROWS_AS(reader.read(buffer), std::out_of_range);

        reader.close();
        std::filesystem::remove_all(directory);
    }

    TEST_CASE("pak.source.OpenMissingFile")
    {
        auto const path = std::filesystem::temp_directory_path() / "pak-io-test-source-missing.pak";
        std::filesystem::remove(path);

        pk::PakReader source;
        CHECK_FALSE(source.open(path));
    }
} // namespace test
