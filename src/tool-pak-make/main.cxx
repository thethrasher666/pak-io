//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-io.hxx"
#include "pak-io/version.hxx"

#include <filesystem>
#include <format>
#include <iostream>
#include <regex>

#include <toml++/toml.h>

namespace
{
    struct manifest_entry
    {
        std::string identifier;   // Uniquely identifies the asset in the archive
        std::string source_path;  // Absolute path on disk
    };

    /// Parse a version string in the format "major.minor"
    auto parse_version(const std::string& version_str, std::uint32_t& major, std::uint32_t& minor) -> bool
    {
        static const std::regex version_regex(R"(^(\d+)\.(\d+)$)");
        std::smatch matches;

        if (!std::regex_match(version_str, matches, version_regex))
        {
            return false;
        }

        try
        {
            major = std::stoul(matches[1].str());
            minor = std::stoul(matches[2].str());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    /// Check if a path pattern contains wildcards
    auto has_wildcards(const std::string& pattern) -> bool
    {
        return pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos;
    }

    /// Convert glob pattern to regex pattern
    auto glob_to_regex(const std::string& glob) -> std::string
    {
        std::string regex_pattern;
        regex_pattern.reserve(glob.size() * 2);

        for (char c : glob)
        {
            switch (c)
            {
                case '*':
                    regex_pattern += ".*";
                    break;
                case '?':
                    regex_pattern += ".";
                    break;
                case '.':
                case '^':
                case '$':
                case '+':
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case '\\':
                case '|':
                    regex_pattern += '\\';
                    regex_pattern += c;
                    break;
                default:
                    regex_pattern += c;
                    break;
            }
        }

        return regex_pattern;
    }

    /// Expand a glob pattern and return matching files
    auto expand_glob(const std::filesystem::path& pattern) -> std::vector<std::filesystem::path>
    {
        std::vector<std::filesystem::path> results;

        // Get the parent directory and filename pattern
        std::filesystem::path parent_path = pattern.parent_path();
        std::string filename_pattern = pattern.filename().string();

        // If parent path is empty, use current directory
        if (parent_path.empty())
        {
            parent_path = ".";
        }

        // Check if parent directory exists
        if (!std::filesystem::exists(parent_path) || !std::filesystem::is_directory(parent_path))
        {
            return results;
        }

        // Convert glob pattern to regex
        std::string regex_pattern = glob_to_regex(filename_pattern);
        std::regex pattern_regex(regex_pattern);

        // Iterate through directory and match files
        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(parent_path))
            {
                if (entry.is_regular_file())
                {
                    std::string filename = entry.path().filename().string();
                    if (std::regex_match(filename, pattern_regex))
                    {
                        results.push_back(entry.path());
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // Ignore errors and return what we have
        }

        return results;
    }

    /// Parse a TOML manifest file
    auto parse_manifest(const std::string& manifest_path) -> std::vector<manifest_entry>
    {
        std::vector<manifest_entry> entries;

        // Get the directory containing the manifest file for resolving relative paths
        std::filesystem::path manifest_dir = std::filesystem::path(manifest_path).parent_path();
        if (manifest_dir.empty())
        {
            manifest_dir = std::filesystem::current_path();
        }

        try
        {
            auto manifest = toml::parse_file(manifest_path);

            // Check version (mandatory)
            auto version_value = manifest["version"].value<std::string>();
            if (!version_value)
            {
                std::cerr << "Error: Manifest must contain a 'version' field\n";
                return entries;
            }

            std::uint32_t major, minor;
            if (!parse_version(*version_value, major, minor))
            {
                std::cerr << std::format("Error: Invalid version format (expected 'major.minor'): {}\n", *version_value);
                return entries;
            }

            if (major != pak::PAK_VERSION_MAJOR || minor != pak::PAK_VERSION_MINOR)
            {
                std::cerr << std::format("Error: Unsupported manifest version {} (expected {}.{})\n",
                           *version_value, pak::PAK_VERSION_MAJOR, pak::PAK_VERSION_MINOR);
                return entries;
            }

            // Get the files array
            auto files = manifest["files"].as_array();
            if (!files)
            {
                std::cerr << "Error: Manifest must contain a 'files' array\n";
                return entries;
            }

            // Parse each file entry
            for (const auto& file_entry : *files)
            {
                auto path_value = file_entry.value<std::string>();

                if (!path_value || path_value->empty())
                {
                    std::cerr << "Warning: File entry is empty\n";
                    continue;
                }

                std::string path_str = *path_value;
                std::filesystem::path file_path(path_str);

                // Only relative paths are supported
                if (!file_path.is_relative())
                {
                    std::cerr << std::format("Warning: Absolute paths are not supported: {}\n", path_str);
                    continue;
                }

                // Make path relative to the manifest file's directory
                file_path = manifest_dir / file_path;

                // Handle wildcards
                if (has_wildcards(path_str))
                {
                    auto matched_files = expand_glob(file_path);
                    if (matched_files.empty())
                    {
                        std::cerr << std::format("Warning: No files matched pattern: {}\n", path_str);
                    }

                    for (const auto& matched_file : matched_files)
                    {
                        manifest_entry entry;
                        entry.source_path = std::filesystem::absolute(matched_file).string();

                        // Compute identifier relative to manifest directory and normalize to forward slashes
                        std::filesystem::path relative = std::filesystem::relative(matched_file, manifest_dir);
                        entry.identifier = relative.generic_string();
                        entries.push_back(entry);
                    }
                }
                else
                {
                    // Single file (no wildcards)
                    manifest_entry entry;
                    entry.source_path = std::filesystem::absolute(file_path).string();

                    // Compute identifier relative to manifest directory and normalize to forward slashes
                    std::filesystem::path relative = std::filesystem::relative(file_path, manifest_dir);
                    entry.identifier = relative.generic_string();
                    entries.push_back(entry);
                }
            }
        }
        catch (const toml::parse_error& err)
        {
            std::cerr << std::format("Error parsing manifest file: {}\n", err.description());
            std::cerr << std::format("  at line {}, column {}\n", err.source().begin.line, err.source().begin.column);
            return entries;
        }
        catch (const std::exception& ex)
        {
            std::cerr << std::format("Error reading manifest file: {}\n", ex.what());
            return entries;
        }

        return entries;
    }

    void print_usage(const char* program_name)
    {
        std::cout << std::format("Usage: {} <manifest_file> <output_pak_file>\n", program_name);
        std::cout << "\n";
        std::cout << "Creates a PAK archive from files listed in a TOML manifest.\n";
        std::cout << "\n";
        std::cout << "Manifest format (TOML):\n";
        std::cout << "  [[files]]\n";
        std::cout << "  \"path/to/file\"          # Relative to manifest file\n";
        std::cout << "  \"data/*.png\"            # Wildcards supported\n";
        std::cout << "\n";
        std::cout << "Example manifest.toml:\n";
        std::cout << "  version = \"1.0\"        # Mandatory: manifest format version\n";
        std::cout << "  compression = \"lz4\"\n";
        std::cout << "\n";
        std::cout << "  [[files]]\n";
        std::cout << "  \"data/level1.dat\"\n";
        std::cout << "\n";
        std::cout << "  [[files]]\n";
        std::cout << "  \"data/*.png\"\n";
    }

} // anonymous namespace

auto main(int32_t argc, char* argv[]) -> int32_t
{
    if (argc != 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    const std::string manifest_path = argv[1];
    const std::string output_path = argv[2];

    // Parse manifest
    std::cout << std::format("Reading manifest: {}\n", manifest_path);
    auto entries = parse_manifest(manifest_path);

    if (entries.empty())
    {
        std::cerr << "Error: No valid entries found in manifest\n";
        return 1;
    }

    std::cout << std::format("Found {} file(s) in manifest\n", entries.size());

    // Create PAK file
    pak::pak_io pak_io;
    if (!pak_io.open(output_path))
    {
        std::cerr << std::format("Error: Could not create PAK file: {}\n", output_path);
        return 1;
    }

    std::cout << std::format("Creating PAK file: {}\n", output_path);

    // Add each file
    std::size_t files_added = 0;
    bool error_occurred = false;
    for (const auto& entry : entries)
    {
        std::cout << std::format("  Adding: {} <- {}\n", entry.identifier, entry.source_path);

        // Check if source file exists
        if (!std::filesystem::exists(entry.source_path))
        {
            std::cerr << std::format("    Error: Source file not found: {}\n", entry.source_path);
            error_occurred = true;
            continue;
        }

        // Add to archive (pak_io will read and compress the file)
        if (!pak_io.add_file(entry.identifier, entry.source_path))
        {
            std::cerr << "    Error: Could not add file to archive\n";
            error_occurred = true;
            continue;
        }

        files_added++;
    }

    // Finalize and close
    if (!pak_io.close())
    {
        std::cerr << "Error: Failed to finalize PAK file\n";
        return 1;
    }

    std::cout << std::format("Successfully created PAK file with {} file(s)\n", files_added);
    return error_occurred ? 1 : 0;
}
