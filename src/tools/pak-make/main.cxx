//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "manifest.hxx"

#include <pak-io/pak-writer.hxx>

#include <cstdlib>
#include <print>
#include <string_view>

namespace pm
{
    void printUsage(std::string_view programName)
    {
        std::println(stdout, "Usage: {} <manifest_file> <output_pak_file>", programName);
    }

    [[nodiscard]] auto entryPoint(std::vector<std::string> const& args) -> int32_t
    {
        if (args.size() != 3)
        {
            printUsage(args[0]);
            return false;
        }

        auto const manifestPath = std::filesystem::path(args[1]);
        auto const outputPath = std::filesystem::path(args[2]);

        auto const result = pm::Manifest::load(manifestPath);
        if (!result)
        {
            std::println(stderr, "Failed to load manifest: {}", result.error().message());
            return false;
        }

        auto const& manifest = *result;
        auto        pakWriter = std::make_unique<pk::PakWriter>();

        if (auto const result = pakWriter->process(outputPath, manifest.files()); !result)
        {
            std::println(stderr, "Failed to write pak file: {}", result.error().message());
            return false;
        }

        return true;
    }
} // namespace pm

auto main(int argc, char** argv) -> int
{
    std::vector<std::string> args(argv, argv + argc);
    return pm::entryPoint(args) ? EXIT_SUCCESS : EXIT_FAILURE;
}
