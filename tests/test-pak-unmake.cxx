//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-io.hxx"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    // Helper to create a temporary directory
    auto create_temp_dir() -> std::filesystem::path
    {
        static std::atomic<int> counter{0};
        auto temp_path = std::filesystem::temp_directory_path() / ("pak_test_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(temp_path);
        return temp_path;
    }

    // Helper to clean up temporary directory
    void cleanup_temp_dir(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path))
        {
#ifdef _WIN32
            // On Windows, files may still be held open briefly after closing
            // Retry a few times with delays
            for (int attempts = 0; attempts < 5; ++attempts)
            {
                try
                {
                    std::filesystem::remove_all(path);
                    break;
                }
                catch (const std::filesystem::filesystem_error&)
                {
                    if (attempts < 4)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    else
                    {
                        throw; // Rethrow on final attempt
                    }
                }
            }
#else
            std::filesystem::remove_all(path);
#endif
        }
    }

    // Helper to create a test file with content
    void create_test_file(const std::filesystem::path& path, const std::string& content)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << content;
    }

    // Helper to read file content
    auto read_file_content(const std::filesystem::path& path) -> std::string
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Helper to create a pak file with known content
    auto create_test_pak(const std::filesystem::path& pak_path) -> std::vector<std::pair<std::string, std::string>>
    {
        std::vector<std::pair<std::string, std::string>> test_files = {
            {"test1.txt", "Hello, World!"},
            {"test2.txt", "This is a test file with more content."},
            {"nested/file.txt", "Nested file content"},
            {"data/config.json", "{\"key\": \"value\"}"}
        };

        pak::pak_io pak(4); // Use 4 worker threads
        REQUIRE(pak.open(pak_path.string()));

        // Create temporary source files
        auto temp_dir = create_temp_dir();
        for (const auto& [identifier, content] : test_files)
        {
            auto file_path = temp_dir / identifier;
            create_test_file(file_path, content);
            REQUIRE(pak.add_file(identifier, file_path.string()));
        }

        REQUIRE(pak.close());
        cleanup_temp_dir(temp_dir);

        return test_files;
    }
}

TEST_CASE("pak_io can open pak file for reading", "[pak-unmake]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "test.pak";

    // Create a test pak file
    auto test_files = create_test_pak(pak_path);

    SECTION("Open existing pak file")
    {
        pak::pak_io pak;
        REQUIRE(pak.open_for_reading(pak_path.string()));
        REQUIRE(pak.is_open());
    }

    SECTION("Fail to open non-existent pak file")
    {
        pak::pak_io pak;
        REQUIRE_FALSE(pak.open_for_reading((temp_dir / "nonexistent.pak").string()));
        REQUIRE_FALSE(pak.is_open());
    }

    cleanup_temp_dir(temp_dir);
}

TEST_CASE("pak_io can list files in pak archive", "[pak-unmake]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "test.pak";

    // Create a test pak file
    auto test_files = create_test_pak(pak_path);

    {
        pak::pak_io pak;
        REQUIRE(pak.open_for_reading(pak_path.string()));

        SECTION("List all files")
        {
            auto files = pak.list_files();
            REQUIRE(files.size() == test_files.size());

            // Verify all expected files are present
            for (const auto& [identifier, _] : test_files)
            {
                REQUIRE(std::find(files.begin(), files.end(), identifier) != files.end());
            }
        }
    } // Ensure pak is destroyed before cleanup

    cleanup_temp_dir(temp_dir);
}

TEST_CASE("pak_io can extract files from pak archive", "[pak-unmake]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "test.pak";
    auto extract_dir = temp_dir / "extracted";

    // Create a test pak file
    auto test_files = create_test_pak(pak_path);

    {
        pak::pak_io pak;
        REQUIRE(pak.open_for_reading(pak_path.string()));

        SECTION("Extract single file")
        {
            auto dest_path = extract_dir / "test1.txt";
            std::filesystem::create_directories(dest_path.parent_path());
            REQUIRE(pak.extract_file("test1.txt", dest_path.string()));
            REQUIRE(std::filesystem::exists(dest_path));

            auto content = read_file_content(dest_path);
            REQUIRE(content == "Hello, World!");
        }

        SECTION("Extract nested file")
        {
            auto dest_path = extract_dir / "nested" / "file.txt";
            std::filesystem::create_directories(dest_path.parent_path());
            REQUIRE(pak.extract_file("nested/file.txt", dest_path.string()));
            REQUIRE(std::filesystem::exists(dest_path));

            auto content = read_file_content(dest_path);
            REQUIRE(content == "Nested file content");
        }

        SECTION("Extract all files")
        {
            for (const auto& [identifier, expected_content] : test_files)
            {
                auto dest_path = extract_dir / identifier;
                std::filesystem::create_directories(dest_path.parent_path());
                REQUIRE(pak.extract_file(identifier, dest_path.string()));
                REQUIRE(std::filesystem::exists(dest_path));

                auto content = read_file_content(dest_path);
                REQUIRE(content == expected_content);
            }
        }

        SECTION("Fail to extract non-existent file")
        {
            auto dest_path = extract_dir / "nonexistent.txt";
            REQUIRE_FALSE(pak.extract_file("nonexistent.txt", dest_path.string()));
        }
    } // Ensure pak is destroyed before cleanup

    cleanup_temp_dir(temp_dir);
}

TEST_CASE("pak_io handles large files correctly", "[pak-unmake]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "large.pak";
    auto extract_dir = temp_dir / "extracted";

    // Create a large file (1 MB)
    std::string large_content(1024 * 1024, 'X');
    for (size_t i = 0; i < large_content.size(); i += 100)
    {
        large_content[i] = 'A' + (i / 100) % 26;
    }

    // Create pak with large file
    pak::pak_io pak_write(4);
    REQUIRE(pak_write.open(pak_path.string()));

    auto temp_file = temp_dir / "large.dat";
    create_test_file(temp_file, large_content);
    REQUIRE(pak_write.add_file("large.dat", temp_file.string()));
    REQUIRE(pak_write.close());

    // Extract and verify
    {
        pak::pak_io pak_read;
        REQUIRE(pak_read.open_for_reading(pak_path.string()));

        auto dest_path = extract_dir / "large.dat";
        std::filesystem::create_directories(dest_path.parent_path());
        REQUIRE(pak_read.extract_file("large.dat", dest_path.string()));

        auto extracted_content = read_file_content(dest_path);
        REQUIRE(extracted_content.size() == large_content.size());
        REQUIRE(extracted_content == large_content);
    } // Ensure pak_read is destroyed before cleanup

    cleanup_temp_dir(temp_dir);
}

TEST_CASE("pak_io handles empty files correctly", "[pak-unmake]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "empty.pak";
    auto extract_dir = temp_dir / "extracted";

    // Create pak with empty file
    pak::pak_io pak_write(4);
    REQUIRE(pak_write.open(pak_path.string()));

    auto temp_file = temp_dir / "empty.txt";
    create_test_file(temp_file, "");
    REQUIRE(pak_write.add_file("empty.txt", temp_file.string()));
    REQUIRE(pak_write.close());

    // Extract and verify
    {
        pak::pak_io pak_read;
        REQUIRE(pak_read.open_for_reading(pak_path.string()));

        auto dest_path = extract_dir / "empty.txt";
        std::filesystem::create_directories(dest_path.parent_path());
        REQUIRE(pak_read.extract_file("empty.txt", dest_path.string()));

        auto extracted_content = read_file_content(dest_path);
        REQUIRE(extracted_content.empty());
    } // Ensure pak_read is destroyed before cleanup

    cleanup_temp_dir(temp_dir);
}

#ifdef PAK_UNMAKE_EXECUTABLE
TEST_CASE("pak-unmake CLI extracts files correctly", "[pak-unmake][cli]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "test.pak";
    auto extract_dir = temp_dir / "extracted";

    // Create a test pak file
    auto test_files = create_test_pak(pak_path);

    // Run pak-unmake
    std::string command = std::string(PAK_UNMAKE_EXECUTABLE) + " " +
                         pak_path.string() + " " + extract_dir.string();
    int result = std::system(command.c_str());
    REQUIRE(result == 0);

    // Verify all files were extracted
    for (const auto& [identifier, expected_content] : test_files)
    {
        auto extracted_path = extract_dir / identifier;
        REQUIRE(std::filesystem::exists(extracted_path));

        auto content = read_file_content(extracted_path);
        REQUIRE(content == expected_content);
    }

    cleanup_temp_dir(temp_dir);
}

TEST_CASE("pak-unmake CLI handles errors correctly", "[pak-unmake][cli]")
{
    auto temp_dir = create_temp_dir();

    SECTION("Missing arguments")
    {
        std::string command = std::string(PAK_UNMAKE_EXECUTABLE);
        int result = std::system(command.c_str());
        REQUIRE(result != 0);
    }

    SECTION("Non-existent pak file")
    {
        std::string command = std::string(PAK_UNMAKE_EXECUTABLE) + " " +
                             (temp_dir / "nonexistent.pak").string() + " " +
                             (temp_dir / "extracted").string();
        int result = std::system(command.c_str());
        REQUIRE(result != 0);
    }

    cleanup_temp_dir(temp_dir);
}
#endif
