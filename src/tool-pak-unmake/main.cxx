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
    if (argc != 3)
    {
        std::cerr << std::format("pak-unmake v{}.{}\n", pak::PAK_VERSION_MAJOR, pak::PAK_VERSION_MINOR);
        std::cerr << std::format("Usage: {} <pak_file> <extraction_directory>\n", argv[0]);
        std::cerr << "\n";
        std::cerr << "Arguments:\n";
        std::cerr << "  pak_file              - Path to the .pak file to extract\n";
        std::cerr << "  extraction_directory  - Directory where files will be extracted\n";
        return 1;
    }

    std::string pak_file_path = argv[1];
    std::string extraction_dir = argv[2];

    // Check if pak file exists
    if (!std::filesystem::exists(pak_file_path))
    {
        std::cerr << std::format("Error: Pak file does not exist: {}\n", pak_file_path);
        return 1;
    }

    // Create extraction directory if it doesn't exist
    try
    {
        std::filesystem::create_directories(extraction_dir);
    }
    catch (const std::exception& e)
    {
        std::cerr << std::format("Error: Failed to create extraction directory: {}\n", e.what());
        return 1;
    }

    // Open pak file
    pak::pak_io pak;
    if (!pak.open_for_reading(pak_file_path))
    {
        std::cerr << std::format("Error: Failed to open pak file: {}\n", pak_file_path);
        return 1;
    }

    // Get list of files
    std::vector<std::string> files = pak.list_files();
    if (files.empty())
    {
        std::cout << "Pak file is empty.\n";
        return 0;
    }

    std::cout << std::format("Extracting {} file(s) from {}...\n", files.size(), pak_file_path);

    // Extract each file
    std::size_t extracted = 0;
    std::size_t failed = 0;

    for (const auto& file_identifier : files)
    {
        // Construct destination path
        std::filesystem::path dest_path = std::filesystem::path(extraction_dir) / file_identifier;

        // Create parent directories if needed
        try
        {
            std::filesystem::create_directories(dest_path.parent_path());
        }
        catch (const std::exception& e)
        {
            std::cerr << std::format("  [FAILED] {} - Cannot create directory: {}\n", file_identifier, e.what());
            ++failed;
            continue;
        }

        // Extract file
        if (pak.extract_file(file_identifier, dest_path.string()))
        {
            std::cout << std::format("  [OK] {}\n", file_identifier);
            ++extracted;
        }
        else
        {
            std::cerr << std::format("  [FAILED] {}\n", file_identifier);
            ++failed;
        }
    }

    std::cout << "\n";
    std::cout << std::format("Extraction complete: {} succeeded, {} failed.\n", extracted, failed);

    return (failed > 0) ? 1 : 0;
}
