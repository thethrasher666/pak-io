//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-error.hxx"

namespace pk
{
    namespace
    {
        /// Custom error category for error codes.
        class PakErrorCategory final : public std::error_category
        {
        public:
            /// Inherited from std::error_category.
            [[nodiscard]] auto name() const noexcept -> char const* final
            {
                return "pak-io::category";
            }

            /// Inherited from std::error_category.
            [[nodiscard]] auto message(int32_t value) const -> std::string final
            {
                auto const errorCode{ static_cast<ErrorCode>(value) };

                switch (errorCode)
                {
                case ErrorCode::NoError:
                    return "No error has occurred.";

                case ErrorCode::FileAlreadyOpenForWriting:
                    return "The file is already open for writing.";

                case ErrorCode::FailedToOpenFileForReading:
                    return "Failed to open the file for reading.";

                case ErrorCode::FailedToReadSourceFile:
                    return "Failed to read one of the files being packed.";

                case ErrorCode::FailedToWriteFile:
                    return "Failed to write the pak file.";

                case ErrorCode::NotAPakFile:
                    return "The file is not a pak file.";

                case ErrorCode::CorruptPakFile:
                    return "The pak file is truncated or its offsets are out of range.";
                }

                return "No error has occurred.";
            }
        };

        [[nodiscard]] auto category() -> std::error_category const&
        {
            static PakErrorCategory instance;
            return instance;
        }
    } // namespace

    auto makeErrorCode(ErrorCode const code) -> std::error_code
    {
        return std::error_code(static_cast<int32_t>(code), category());
    }
} // namespace pk
