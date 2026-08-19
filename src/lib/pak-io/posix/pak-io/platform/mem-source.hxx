//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>

namespace pk
{
    /// A platform-specific memory-mapped file source implementation.
    class MemSourcePlatform
    {
    public:
        ~MemSourcePlatform();

        /// Opens a memory-mapped file source.
        /// \param path The path to the memory-mapped file to open.
        /// \return void if the memory-mapped file was opened successfully, an error otherwise.
        [[nodiscard]] auto open(std::filesystem::path const& path) -> std::expected<void, std::error_code>;

        /// Closes the memory-mapped file source.
        void close();

        /// \return The size of the mapped file, in bytes.
        [[nodiscard]] auto size() const noexcept -> std::size_t;

        /// \return The current read position, in bytes from the start of the file.
        [[nodiscard]] auto position() const noexcept -> std::size_t;

        /// Moves the read position.
        /// \param position The new read position, in bytes from the start of the file.
        void seek(std::size_t position);

        /// \return A read-only view of the whole mapped file.
        [[nodiscard]] auto view() const noexcept -> std::span<std::byte const>;

        /// Read an arbitrary amount of data from the current position, advancing it.
        /// \param data The buffer to read into.
        /// \param size The size of the buffer.
        void read(void* data, std::size_t size);

        /// Read an arbitrary amount of data from an absolute offset, leaving the current position untouched.
        /// \param data The buffer to read into.
        /// \param size The size of the buffer.
        /// \param offset The offset to read from, in bytes from the start of the file.
        void readAt(void* data, std::size_t size, std::size_t offset) const;

    private:
        int         _file{ -1 };
        void*       _view{};
        std::size_t _size{};
        std::size_t _position{};
    };
} // namespace pk
