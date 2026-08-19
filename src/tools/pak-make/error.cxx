//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "error.hxx"

#include <format>

namespace pm
{
    namespace
    {
        /// Custom error category for error codes.
        class PakMakeErrorCategory final : public std::error_category
        {
        public:
            /// Inherited from std::error_category.
            [[nodiscard]] auto name() const noexcept -> char const* final
            {
                return "pak-make::category";
            }

            /// Inherited from std::error_category.
            [[nodiscard]] auto message(int32_t value) const -> std::string final
            {
                auto const errorCode{ static_cast<ErrorCode>(value) };

                switch (errorCode)
                {
                case ErrorCode::NoError:
                    return "No error has occurred.";

                case ErrorCode::InvalidVersionIdentifier:
                    return "The version identifier in the manifest is invalid.";

                case ErrorCode::ToolchainVersionTooOld:
                    return "The toolchain version is too old to read or write the PAK file.";

                case ErrorCode::NoFilesArray:
                    return "The manifest does not contain a 'files' array.";

                case ErrorCode::FilePathIsEmpty:
                    return "A file path in the manifest is empty.";

                case ErrorCode::ManifestParseFailed:
                    return "The manifest could not be parsed as TOML.";
                }

                return "No error has occurred.";
            }
        };

        [[nodiscard]] auto category() -> std::error_category const&
        {
            static PakMakeErrorCategory instance;
            return instance;
        }
    } // namespace

    auto makeErrorCode(ErrorCode const code) -> std::error_code
    {
        return std::error_code(static_cast<int32_t>(code), category());
    }
} // namespace pm
