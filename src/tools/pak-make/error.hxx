//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <string>
#include <system_error>

namespace pm
{
    /// The possible errors relating to the reading or writing of PAK files.
    enum class ErrorCode
    {
        NoError,                  ///< No error has occurred.
        InvalidVersionIdentifier, ///< The version identifier in the manifest is invalid.
        ToolchainVersionTooOld,   ///< The toolchain version is too old to read or write the PAK file.
        NoFilesArray,             ///< The manifest does not contain a 'files' array.
        FilePathIsEmpty,          ///< A file path in the manifest is empty.
        ManifestParseFailed       ///< The manifest could not be parsed as TOML.
    };

    /// Create an error code.
    /// \param code The pak-io related error code.
    /// \return A valid error code.
    [[nodiscard]] auto makeErrorCode(ErrorCode const code) -> std::error_code;
} // namespace pm
