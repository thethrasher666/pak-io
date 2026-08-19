//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include "error.hxx"

#include <expected>
#include <filesystem>
#include <system_error>
#include <vector>
#include <toml++/toml.h>

namespace pm
{
    /// The manifest is a file containing what files to include in a PAK file, and where to place them.
    class Manifest final
    {
    public:
        /// Get the file list.
        /// \return The list of files to include in the PAK file.
        [[nodiscard]] auto files() const -> std::vector<std::filesystem::path> const&
        {
            return _files;
        }

        /// Loads a manifest from a TOML file.
        /// \param path The path to the TOML file to load.
        /// \return The loaded manifest, or an error if the manifest could not be loaded.
        [[nodiscard]] static auto load(std::filesystem::path const& path) -> std::expected<Manifest, std::error_code>;

    private:
        /// Parses the version string from the manifest.
        /// \param version The version string to parse.
        /// \return True if the version string was parsed successfully, false otherwise.
        [[nodiscard]] auto parseVersion(std::string const& version) -> bool;

    private:
        uint32_t                           _versionMajor{};
        uint32_t                           _versionMinor{};
        std::vector<std::filesystem::path> _files;
    };
} // namespace pm
