//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "manifest.hxx"
#include "error.hxx"

#include <charconv>
#include <format>
#include <regex>
#include <string_view>
#include <pak-io/version.hxx>

namespace pm
{
    namespace
    {
        auto parseComponent(std::string_view const text, std::uint32_t& value) -> bool
        {
            auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size();
        }
    } // namespace

    auto Manifest::load(std::filesystem::path const& path) -> std::expected<Manifest, std::error_code>
    {
        auto result = toml::parse_file(path.c_str());

        if (!result)
        {
            return std::unexpected(makeErrorCode(ErrorCode::ManifestParseFailed));
        }

        auto const table = std::move(result).table();

        Manifest manifest;

        if (auto const version = table["version"].value<std::string>())
        {
            if (!manifest.parseVersion(*version))
            {
                return std::unexpected(makeErrorCode(ErrorCode::InvalidVersionIdentifier));
            }

            if (manifest._versionMajor < pk::version::major() || manifest._versionMinor < pk::version::minor())
            {
                return std::unexpected(makeErrorCode(ErrorCode::ToolchainVersionTooOld));
            }
        }

        auto const* const files = table["files"].as_array();
        if (!files)
        {
            return std::unexpected(makeErrorCode(ErrorCode::NoFilesArray));
        }

        auto const manifestDirectory = path.parent_path();

        for (auto const& fileEntry : *files)
        {
            auto const pathValue = fileEntry.value<std::string>();
            if (!pathValue || pathValue->empty())
            {
                return std::unexpected(makeErrorCode(ErrorCode::FilePathIsEmpty));
            }

            std::filesystem::path filePath(*pathValue);

            // Relative entries are resolved against the manifest's own directory.
            if (filePath.is_relative())
            {
                filePath = manifestDirectory / filePath;
            }

            manifest._files.emplace_back(std::move(filePath));
        }

        return manifest;
    }

    auto Manifest::parseVersion(std::string const& version) -> bool
    {
        static const std::regex version_regex(R"(^(\d+)\.(\d+)$)");
        std::smatch             matches;

        if (!std::regex_match(version, matches, version_regex))
        {
            return false;
        }

        auto const major = matches[1].str();
        auto const minor = matches[2].str();

        // The regex guarantees digits, so from_chars can only fail on overflow.
        return parseComponent(major, _versionMajor) && parseComponent(minor, _versionMinor);
    }
} // namespace pm
