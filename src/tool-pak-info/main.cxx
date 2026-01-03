//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-io.hxx"
#include "pak-io/version.hxx"

#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>

auto main(int32_t argc, char* argv[]) -> int32_t
{
    // Check arguments
    if (argc != 2)
    {
        std::cerr << std::format("pak-info v{}.{}\n", pak::PAK_VERSION_MAJOR, pak::PAK_VERSION_MINOR);
        std::cerr << std::format("Usage: {} <pak_file>\n", argv[0]);
        std::cerr << "\n";
        std::cerr << "Arguments:\n";
        std::cerr << "  pak_file  - Path to the .pak file to inspect\n";
        return 1;
    }

    std::string pak_file_path = argv[1];

    // Check if pak file exists
    if (!std::filesystem::exists(pak_file_path))
    {
        std::cerr << std::format("Error: Pak file does not exist: {}\n", pak_file_path);
        return 1;
    }

    // Open pak file for reading
    pak::pak_io pak;
    if (!pak.open_for_reading(pak_file_path))
    {
        std::cerr << std::format("Error: Failed to open pak file: {}\n", pak_file_path);
        std::cerr << "This may not be a valid pak file or it may be corrupted.\n";
        return 1;
    }

    // Get file information
    auto file_size = std::filesystem::file_size(pak_file_path);
    auto file_infos = pak.get_file_info();

    // Calculate totals
    std::uint64_t total_compressed = 0;
    std::uint64_t total_uncompressed = 0;
    for (const auto& info : file_infos)
    {
        total_compressed += info.compressed_size;
        total_uncompressed += info.uncompressed_size;
    }

    // Display header
    std::cout << "PAK Archive Information\n";
    std::cout << "=======================\n";
    std::cout << "\n";
    std::cout << std::format("File: {}\n", pak_file_path);
    std::cout << std::format("Archive Size: {} bytes ({:.2f} KB, {:.2f} MB)\n",
                file_size,
                file_size / 1024.0,
                file_size / (1024.0 * 1024.0));
    std::cout << std::format("Files: {}\n", file_infos.size());
    std::cout << std::format("Total Uncompressed: {} bytes ({:.2f} KB, {:.2f} MB)\n",
                total_uncompressed,
                total_uncompressed / 1024.0,
                total_uncompressed / (1024.0 * 1024.0));
    std::cout << std::format("Total Compressed: {} bytes ({:.2f} KB, {:.2f} MB)\n",
                total_compressed,
                total_compressed / 1024.0,
                total_compressed / (1024.0 * 1024.0));
    if (total_uncompressed > 0)
    {
        double overall_ratio = (1.0 - static_cast<double>(total_compressed) / static_cast<double>(total_uncompressed)) * 100.0;
        std::cout << std::format("Overall Compression: {:.1f}%\n", overall_ratio);
    }
    std::cout << "\n";

    if (file_infos.empty())
    {
        std::cout << "Archive is empty.\n";
        return 0;
    }

    // Display file listing
    std::cout << "Contents:\n";
    std::cout << "=========\n";
    std::cout << "\n";

    // Calculate column widths for nice formatting
    std::size_t max_name_length = 0;
    for (const auto& info : file_infos)
    {
        max_name_length = std::max(max_name_length, info.name.length());
    }
    max_name_length = std::max(max_name_length, std::size_t(20)); // Minimum width

    // Print header
    std::cout << std::format("{:<{}}  {:>15}  {:>15}  {:>8}  {:>10}\n",
                "Filename", max_name_length,
                "Compressed", "Uncompressed", "Ratio", "Offset");
    std::cout << std::format("{:-<{}}  {:->15}  {:->15}  {:->8}  {:->10}\n",
                "", max_name_length, "", "", "", "");

    // Print each file
    for (const auto& info : file_infos)
    {
        double ratio = 0.0;
        if (info.uncompressed_size > 0)
        {
            ratio = (1.0 - static_cast<double>(info.compressed_size) / static_cast<double>(info.uncompressed_size)) * 100.0;
        }

        std::cout << std::format("{:<{}}  {:>15}  {:>15}  {:>7.1f}%  {:>10}\n",
                    info.name, max_name_length,
                    info.compressed_size,
                    info.uncompressed_size,
                    ratio,
                    info.offset);
    }

    std::cout << "\n";
    std::cout << std::format("Total: {} file(s)\n", file_infos.size());

    return 0;
}
