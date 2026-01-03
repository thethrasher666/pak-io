//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-io.hxx"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#ifdef _WIN32
    // Windows doesn't have WEXITSTATUS - pclose returns exit code directly
    #define GET_EXIT_CODE(status) (status)
#else
    #include <sys/wait.h>
    #define GET_EXIT_CODE(status) WEXITSTATUS(status)
#endif

namespace
{
    // Helper to create a temporary directory
    auto create_temp_dir() -> std::filesystem::path
    {
        static std::atomic<int> counter{0};
        auto temp_path = std::filesystem::temp_directory_path() /
                        ("pak_info_test_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(temp_path);
        return temp_path;
    }

    // Helper to clean up temporary directory
    void cleanup_temp_dir(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path))
        {
            std::filesystem::remove_all(path);
        }
    }

    // Helper to create a test file with content
    void create_test_file(const std::filesystem::path& path, const std::string& content)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << content;
    }

    // Helper to create a pak file with known content
    auto create_test_pak_with_files(const std::filesystem::path& pak_path,
                                    const std::vector<std::pair<std::string, std::string>>& files) -> bool
    {
        pak::pak_io pak(4);
        if (!pak.open(pak_path.string()))
        {
            return false;
        }

        auto temp_dir = create_temp_dir();
        for (const auto& [identifier, content] : files)
        {
            auto file_path = temp_dir / identifier;
            create_test_file(file_path, content);
            if (!pak.add_file(identifier, file_path.string()))
            {
                cleanup_temp_dir(temp_dir);
                return false;
            }
        }

        bool success = pak.close();
        cleanup_temp_dir(temp_dir);
        return success;
    }

    // Helper to run pak-info and capture output
    auto run_pak_info(const std::string& pak_path) -> std::pair<int, std::string>
    {
#ifdef PAK_INFO_EXECUTABLE
        std::string command = std::string(PAK_INFO_EXECUTABLE) + " \"" + pak_path + "\" 2>&1";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe)
        {
            return {-1, ""};
        }

        std::stringstream output;
        char buffer[4096];  // Larger buffer to reduce system calls
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            output << buffer;
        }

        int exit_code = pclose(pipe);
        return {GET_EXIT_CODE(exit_code), output.str()};
#else
        return {-1, "PAK_INFO_EXECUTABLE not defined"};
#endif
    }
}

#ifdef PAK_INFO_EXECUTABLE
TEST_CASE("pak-info CLI displays archive information correctly", "[pak-info][cli]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "test.pak";

    SECTION("Display information for pak with multiple files")
    {
        // Create pak with known files
        std::vector<std::pair<std::string, std::string>> files = {
            {"file1.txt", "Hello, World!"},
            {"file2.txt", "This is a longer test file with more content to compress."},
            {"data/config.json", "{\"key\": \"value\"}"}
        };

        REQUIRE(create_test_pak_with_files(pak_path, files));

        auto [exit_code, output] = run_pak_info(pak_path.string());

        REQUIRE(exit_code == 0);
        REQUIRE(output.find("PAK Archive Information") != std::string::npos);
        REQUIRE(output.find("Files: 3") != std::string::npos);
        REQUIRE(output.find("file1.txt") != std::string::npos);
        REQUIRE(output.find("file2.txt") != std::string::npos);
        REQUIRE(output.find("data/config.json") != std::string::npos);
        REQUIRE(output.find("Total: 3 file(s)") != std::string::npos);
    }

    SECTION("Display information for empty pak")
    {
        // Create empty pak
        REQUIRE(create_test_pak_with_files(pak_path, {}));

        auto [exit_code, output] = run_pak_info(pak_path.string());

        REQUIRE(exit_code == 0);
        REQUIRE(output.find("Files: 0") != std::string::npos);
        REQUIRE(output.find("Archive is empty") != std::string::npos);
    }

    SECTION("Display sizes and compression ratios")
    {
        // Create pak with compressible data
        std::string repeated_data(1000, 'A'); // Very compressible
        std::vector<std::pair<std::string, std::string>> files = {
            {"repeated.txt", repeated_data}
        };

        REQUIRE(create_test_pak_with_files(pak_path, files));

        auto [exit_code, output] = run_pak_info(pak_path.string());

        REQUIRE(exit_code == 0);
        // Should show compression ratio
        REQUIRE(output.find("Overall Compression:") != std::string::npos);
        REQUIRE(output.find("Compressed") != std::string::npos);
        REQUIRE(output.find("Uncompressed") != std::string::npos);
        REQUIRE(output.find("Ratio") != std::string::npos);
    }

    cleanup_temp_dir(temp_dir);
}

TEST_CASE("pak-info CLI handles errors correctly", "[pak-info][cli]")
{
    auto temp_dir = create_temp_dir();

    SECTION("Missing arguments")
    {
#ifdef PAK_INFO_EXECUTABLE
        std::string command = std::string(PAK_INFO_EXECUTABLE) + " 2>&1";
        FILE* pipe = popen(command.c_str(), "r");
        REQUIRE(pipe != nullptr);

        std::stringstream output;
        char buffer[4096];  // Larger buffer to reduce system calls
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            output << buffer;
        }

        int status = pclose(pipe);
        int exit_code = GET_EXIT_CODE(status);
        std::string output_str = output.str();

        REQUIRE(exit_code != 0);
        REQUIRE(output_str.find("Usage:") != std::string::npos);
#endif
    }

    SECTION("Non-existent pak file")
    {
        auto non_existent = temp_dir / "nonexistent.pak";
        auto [exit_code, output] = run_pak_info(non_existent.string());

        REQUIRE(exit_code != 0);
        REQUIRE(output.find("does not exist") != std::string::npos);
    }

    SECTION("Invalid pak file")
    {
        // Create a file that's not a valid pak
        auto invalid_pak = temp_dir / "invalid.pak";
        std::ofstream file(invalid_pak);
        file << "This is not a valid pak file";
        file.close();

        auto [exit_code, output] = run_pak_info(invalid_pak.string());

        REQUIRE(exit_code != 0);
        REQUIRE(output.find("Failed to open pak file") != std::string::npos);
    }

    cleanup_temp_dir(temp_dir);
}

TEST_CASE("pak-info CLI output format is consistent", "[pak-info][cli]")
{
    auto temp_dir = create_temp_dir();
    auto pak_path = temp_dir / "format_test.pak";

    std::vector<std::pair<std::string, std::string>> files = {
        {"short.txt", "Hi"},
        {"verylongfilenamewithlotsofcharacters.txt", "Content"},
        {"nested/deep/path/file.dat", "Data"}
    };

    REQUIRE(create_test_pak_with_files(pak_path, files));

    auto [exit_code, output] = run_pak_info(pak_path.string());

    REQUIRE(exit_code == 0);

    // Check that output contains table headers
    REQUIRE(output.find("Filename") != std::string::npos);
    REQUIRE(output.find("Compressed") != std::string::npos);
    REQUIRE(output.find("Uncompressed") != std::string::npos);
    REQUIRE(output.find("Ratio") != std::string::npos);
    REQUIRE(output.find("Offset") != std::string::npos);

    // Check that all files are listed
    REQUIRE(output.find("short.txt") != std::string::npos);
    REQUIRE(output.find("verylongfilenamewithlotsofcharacters.txt") != std::string::npos);
    REQUIRE(output.find("nested/deep/path/file.dat") != std::string::npos);

    cleanup_temp_dir(temp_dir);
}
#endif // PAK_INFO_EXECUTABLE
