//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include <pak-io/pak-reader.hxx>

#include <cstdlib>
#include <fstream>
#include <print>
#include <string_view>

namespace pu
{
    void printUsage(std::string_view programName)
    {
        std::println(stdout, "Usage: {} <pak_file> <output_directory>", programName);
    }

    [[nodiscard]] auto entryPoint(std::vector<std::string> const& args) -> int32_t
    {
        if (args.size() != 3)
        {
            printUsage(args[0]);
            return false;
        }

        auto const inputPath = std::filesystem::path(args[1]);
        auto const outputPath = std::filesystem::path(args[2]);

        auto pakReader = std::make_unique<pk::PakReader>();
        if (auto const result = pakReader->open(inputPath); !result)
        {
            std::println(stderr, "Failed to open pak file: {}", result.error().message());
            return false;
        }

        std::error_code error;
        std::filesystem::create_directories(outputPath, error);
        if (error)
        {
            std::println(stderr, "Failed to create output directory: {}", error.message());
            return false;
        }

        for (auto const& entry : pakReader->entries())
        {
            auto const entryPath = outputPath / entry.filename;

            std::ofstream file(entryPath, std::ios::binary);
            if (!file.good())
            {
                std::println(stderr, "Failed to open file for writing: {}", entryPath.string());
                return false;
            }

            file.write(reinterpret_cast<char const*>(entry.data), static_cast<std::streamsize>(entry.size));
            if (!file.good())
            {
                std::println(stderr, "Failed to write file: {}", entryPath.string());
                return false;
            }
        }

        return true;
    }
} // namespace pu

auto main(int argc, char** argv) -> int
{
    std::vector<std::string> args(argv, argv + argc);
    return pu::entryPoint(args) ? EXIT_SUCCESS : EXIT_FAILURE;
}
