//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-io.hxx"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace
{
    /// Helper to create a test file with specified content
    auto create_test_file(const std::filesystem::path& path, const std::string& content) -> void
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(content.data(), content.size());
        file.close();
    }

    /// Helper to create a manifest file
    auto create_manifest(const std::filesystem::path& path, const std::vector<std::string>& file_paths) -> void
    {
        std::ofstream file(path);
        file << "version = \"1.0\"\n";
        file << "compression = \"lz4\"\n\n";
        for (const auto& file_path : file_paths)
        {
            file << "[[files]]\n";
            file << "\"" << file_path << "\"\n\n";
        }
        file.close();
    }
} // namespace

SCENARIO("pak_io creates archives with compressed data", "[pak_io]")
{
    GIVEN("test files and a temporary directory")
    {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        std::filesystem::path test_file1 = temp_dir / "file1.txt";
        std::filesystem::path test_file2 = temp_dir / "file2.dat";
        std::filesystem::path pak_file = temp_dir / "test.pak";

        const std::string content1 = "This is test file 1 with some compressible content!";
        const std::string content2 = "Binary data: \x00\x01\x02\x03\xFF\xFE\xFD";

        create_test_file(test_file1, content1);
        create_test_file(test_file2, content2);

        WHEN("creating a PAK archive with multiple files")
        {
            pak::pak_io pak_io;
            REQUIRE(pak_io.open(pak_file.string()));

            REQUIRE(pak_io.add_file("data/file1.txt", test_file1.string()));
            REQUIRE(pak_io.add_file("data/file2.dat", test_file2.string()));
            REQUIRE(pak_io.close());

            THEN("the PAK file exists")
            {
                REQUIRE(std::filesystem::exists(pak_file));
            }

            AND_THEN("the PAK file has non-zero size")
            {
                REQUIRE(std::filesystem::file_size(pak_file) > 0);
            }
        }

        std::filesystem::remove_all(temp_dir);
    }
}

SCENARIO("pak_io handles empty archive", "[pak_io]")
{
    GIVEN("a temporary directory")
    {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_empty_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        std::filesystem::path pak_file = temp_dir / "empty.pak";

        WHEN("creating an empty PAK archive")
        {
            pak::pak_io pak_io;
            REQUIRE(pak_io.open(pak_file.string()));
            REQUIRE(pak_io.close());

            THEN("the PAK file exists")
            {
                REQUIRE(std::filesystem::exists(pak_file));
            }

            AND_THEN("the PAK file contains at least the footer")
            {
                // Footer is 24 bytes: magic(4) + version(4) + entry_count(8) + table_offset(8)
                REQUIRE(std::filesystem::file_size(pak_file) >= 24);
            }
        }

        std::filesystem::remove_all(temp_dir);
    }
}

SCENARIO("pak_io handles large files", "[pak_io]")
{
    GIVEN("a large test file")
    {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_large_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        std::filesystem::path large_file = temp_dir / "large.bin";
        std::filesystem::path pak_file = temp_dir / "large.pak";

        // Create a 1MB file with repeating pattern (highly compressible)
        const std::size_t file_size = 1024 * 1024;
        std::vector<std::uint8_t> large_data(file_size);
        for (std::size_t i = 0; i < file_size; ++i)
        {
            large_data[i] = static_cast<std::uint8_t>(i % 256);
        }
        create_test_file(large_file, std::string(reinterpret_cast<char*>(large_data.data()), large_data.size()));

        WHEN("adding a large file to the archive")
        {
            pak::pak_io pak_io;
            REQUIRE(pak_io.open(pak_file.string()));
            REQUIRE(pak_io.add_file("large.bin", large_file.string()));

            THEN("closing succeeds")
            {
                REQUIRE(pak_io.close());
            }

            AND_THEN("the compressed PAK is smaller than original")
            {
                // With LZ4 compression, repeating pattern should compress well
                std::size_t pak_size = std::filesystem::file_size(pak_file);
                REQUIRE(pak_size < file_size);
            }
        }

        std::filesystem::remove_all(temp_dir);
    }
}

SCENARIO("pak_io handles error conditions", "[pak_io]")
{
    GIVEN("a pak_io instance")
    {
        pak::pak_io pak_io;

        WHEN("adding files without opening")
        {
            bool result = pak_io.add_file("test.txt", "/nonexistent/file.txt");

            THEN("add_file fails")
            {
                REQUIRE(result == false);
            }
        }

        WHEN("adding file with empty identifier")
        {
            std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_error_test";
            std::filesystem::remove_all(temp_dir);
            std::filesystem::create_directories(temp_dir);
            std::filesystem::path pak_file = temp_dir / "test.pak";
            std::filesystem::path test_file = temp_dir / "test.txt";
            create_test_file(test_file, "test");

            REQUIRE(pak_io.open(pak_file.string()));

            bool result = pak_io.add_file("", test_file.string());

            THEN("add_file fails")
            {
                REQUIRE(result == false);
            }

            REQUIRE(pak_io.close());
            std::filesystem::remove_all(temp_dir);
        }

        WHEN("adding file with empty source path")
        {
            std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_error_test2";
            std::filesystem::remove_all(temp_dir);
            std::filesystem::create_directories(temp_dir);
            std::filesystem::path pak_file = temp_dir / "test.pak";

            REQUIRE(pak_io.open(pak_file.string()));

            bool result = pak_io.add_file("test.txt", "");

            THEN("add_file fails")
            {
                REQUIRE(result == false);
            }

            REQUIRE(pak_io.close());
            std::filesystem::remove_all(temp_dir);
        }

        WHEN("closing without opening")
        {
            bool result = pak_io.close();

            THEN("close succeeds (no-op)")
            {
                REQUIRE(result == true);
            }
        }
    }
}

SCENARIO("pak_io handles multiple archives", "[pak_io]")
{
    GIVEN("multiple test files")
    {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_multi_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        std::filesystem::path test_file = temp_dir / "test.txt";
        std::filesystem::path pak_file1 = temp_dir / "archive1.pak";
        std::filesystem::path pak_file2 = temp_dir / "archive2.pak";

        const std::string content = "Shared test content";
        create_test_file(test_file, content);

        WHEN("creating multiple archives sequentially")
        {
            pak::pak_io pak_io;

            // First archive
            REQUIRE(pak_io.open(pak_file1.string()));
            REQUIRE(pak_io.add_file("file.txt", test_file.string()));
            REQUIRE(pak_io.close());

            // Second archive
            REQUIRE(pak_io.open(pak_file2.string()));
            REQUIRE(pak_io.add_file("file.txt", test_file.string()));
            REQUIRE(pak_io.close());

            THEN("both PAK files exist")
            {
                REQUIRE(std::filesystem::exists(pak_file1));
                REQUIRE(std::filesystem::exists(pak_file2));
            }

            AND_THEN("both PAK files have non-zero size")
            {
                REQUIRE(std::filesystem::file_size(pak_file1) > 0);
                REQUIRE(std::filesystem::file_size(pak_file2) > 0);
            }
        }

        std::filesystem::remove_all(temp_dir);
    }
}

SCENARIO("pak_io handles special characters in filenames", "[pak_io]")
{
    GIVEN("files with special characters in archive paths")
    {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_special_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        std::filesystem::path pak_file = temp_dir / "special.pak";
        std::filesystem::path test_file = temp_dir / "test.txt";
        const std::string content = "Test content";
        create_test_file(test_file, content);

        WHEN("adding files with various path formats")
        {
            pak::pak_io pak_io;
            REQUIRE(pak_io.open(pak_file.string()));

            REQUIRE(pak_io.add_file("data/subfolder/file.txt", test_file.string()));
            REQUIRE(pak_io.add_file("file-with-dashes.txt", test_file.string()));
            REQUIRE(pak_io.add_file("file_with_underscores.txt", test_file.string()));
            REQUIRE(pak_io.add_file("file.with.dots.txt", test_file.string()));

            THEN("closing succeeds")
            {
                REQUIRE(pak_io.close());
            }

            AND_THEN("the PAK file exists")
            {
                REQUIRE(std::filesystem::exists(pak_file));
            }
        }

        std::filesystem::remove_all(temp_dir);
    }
}

SCENARIO("pak_io handles concurrent file additions via worker threads", "[pak_io]")
{
    GIVEN("multiple test files to add concurrently")
    {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "pak_io_concurrent_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        std::filesystem::path pak_file = temp_dir / "concurrent.pak";

        // Create test files
        std::vector<std::filesystem::path> test_files;
        for (int i = 0; i < 10; ++i)
        {
            std::string content = "File " + std::to_string(i) + " content with some data to compress";
            std::filesystem::path file_path = temp_dir / ("file" + std::to_string(i) + ".txt");
            create_test_file(file_path, content);
            test_files.push_back(file_path);
        }

        WHEN("adding multiple files that will be compressed in parallel")
        {
            pak::pak_io pak_io(4); // Use 4 worker threads
            REQUIRE(pak_io.open(pak_file.string()));

            // Add multiple files quickly - they should be queued and compressed in parallel
            for (int i = 0; i < 10; ++i)
            {
                std::string filename = "file" + std::to_string(i) + ".txt";
                REQUIRE(pak_io.add_file(filename, test_files[i].string()));
            }

            THEN("closing waits for all compression jobs and succeeds")
            {
                REQUIRE(pak_io.close());

                AND_THEN("the PAK file exists and has expected content")
                {
                    REQUIRE(std::filesystem::exists(pak_file));
                    REQUIRE(std::filesystem::file_size(pak_file) > 0);
                }
            }
        }

        std::filesystem::remove_all(temp_dir);
    }
}

SCENARIO("tool-pak-make handles missing source files", "[tool-pak-make]")
{
    GIVEN("a manifest referencing non-existent files")
    {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "tool_pak_make_missing_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        std::filesystem::path manifest = temp_dir / "manifest.toml";
        std::filesystem::path pak_file = temp_dir / "output.pak";

        create_manifest(manifest, {"nonexistent/file.txt"});

        WHEN("invoking tool-pak-make")
        {
            std::string command = PAK_MAKE_EXECUTABLE + std::string(" ") + manifest.string() + " " + pak_file.string();
            int result = std::system(command.c_str());

            THEN("the tool fails")
            {
                REQUIRE(result != 0);
            }
        }

        std::filesystem::remove_all(temp_dir);
    }
}

SCENARIO("tool-pak-make works with the sample manifest", "[tool-pak-make][sample]")
{
    GIVEN("the sample.toml manifest with test data files")
    {
        // Use the actual sample.toml from the tests directory
        std::filesystem::path manifest(TEST_MANIFEST_FILE);
        std::filesystem::path manifest_dir = manifest.parent_path();
        std::filesystem::path pak_file = manifest_dir / "output_sample.pak";

        // Clean up any previous test output
        std::filesystem::remove(pak_file);

        WHEN("invoking tool-pak-make with the sample manifest")
        {
            std::string command = PAK_MAKE_EXECUTABLE + std::string(" ") + manifest.string() + " " + pak_file.string();
            int result = std::system(command.c_str());

            THEN("the tool succeeds")
            {
                REQUIRE(result == 0);
            }

            AND_THEN("the PAK file is created")
            {
                REQUIRE(std::filesystem::exists(pak_file));
                REQUIRE(std::filesystem::file_size(pak_file) > 0);
            }

            // Clean up
            std::filesystem::remove(pak_file);
        }
    }
}
