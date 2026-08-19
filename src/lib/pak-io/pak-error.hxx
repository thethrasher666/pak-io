//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <cstdint>
#include <system_error>

namespace pk
{
    /// The possible errors relating to the reading or writing of PAK files.
    enum class ErrorCode
    {
        NoError,                    ///< No error has occurred.
        FileAlreadyOpenForWriting,  ///< The file is already open for writing.
        FailedToOpenFileForReading, ///< Failed to open the file.
        FailedToReadSourceFile,     ///< Failed to read one of the files being packed.
        FailedToWriteFile,          ///< Failed to write the pak file.
        NotAPakFile,                ///< The file is not a pak file.
        CorruptPakFile              ///< The pak file is truncated or its offsets are out of range.
    };

    /// Create an error code.
    /// \param code The pak-io related error code.
    /// \return A valid error code.
    [[nodiscard]] auto makeErrorCode(ErrorCode const code) -> std::error_code;
} // namespace pk
