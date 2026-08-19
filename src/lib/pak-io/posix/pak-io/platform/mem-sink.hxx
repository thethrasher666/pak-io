//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <system_error>

namespace pk
{
    /// A platform-specific memory-mapped file sink implementation.
    class MemSinkPlatform
    {
    public:
        ~MemSinkPlatform();

        /// Opens a memory-mapped file sink.
        /// \param path The path to the memory-mapped file to open.
        /// \return void if the memory-mapped file was opened successfully, an error otherwise.
        [[nodiscard]] auto open(std::filesystem::path const& path) -> std::expected<void, std::error_code>;

        /// Closes the memory-mapped file sink.
        void close();

        /// Write an arbitrary amount of data.
        /// \param data The data to write.
        /// \param size The size of the data to write.
        void write(void const* data, std::size_t size);

    private:
        [[nodiscard]] auto resizeAndMap(std::size_t size) -> std::expected<void, std::error_code>;

        int         _file{ -1 };
        void*       _view{};
        std::size_t _capacity{};
        std::size_t _position{};
        std::size_t _pageSize{};
    };
} // namespace pk
