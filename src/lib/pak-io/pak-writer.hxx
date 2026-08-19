//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <system_error>
#include <vector>

namespace pk
{
    class MemSinkPlatform;

    /// A class for creating/writing PAK file, using memory-mapped I/O.
    class PakWriter final
    {
    public:
        /// Constructor.
        PakWriter();

        /// Destructor.
        ~PakWriter();

        /// Closes the PAK file.
        void close();

        /// Opens a PAK file for writing.
        /// \param path The path to the PAK file to open.
        /// \param files The list of files to include in the PAK file.
        /// \return void if the PAK file was opened successfully, an error otherwise.
        [[nodiscard]] auto process(std::filesystem::path const& path, std::vector<std::filesystem::path> const& files) -> std::expected<void, std::error_code>;

    private:
        /// Write an arbitrary amount of data.
        /// \param data The data to write.
        /// \param size The size of the data to write.
        void write(void const* data, std::size_t size);

        /// Write the contents of a container-like object to the memory-mapped file.
        template <typename T>
        void write(const std::vector<T>& data)
        {
            write(data.data(), data.size() * sizeof(T));
        }

    private:
        std::unique_ptr<MemSinkPlatform> _memSink; ///< The platform-specific implementation of the PAK sink.
    };
} // namespace pk
