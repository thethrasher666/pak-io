//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pak
{
    /// memory_map provides the capabilities to read and write compressed compound archives of "hero" files using memory mapping.
    class memory_map
    {
    public:
        /// Mode for opening the memory mapped file
        enum class Mode
        {
            read_only, ///< Open for reading only
            read_write ///< Open for reading and writing (creates if doesn't exist)
        };

        /// Constructor
        memory_map();

        /// Destructor - ensures resources are properly released
        ~memory_map();

        // Delete copy constructor
        memory_map(const memory_map&) = delete;

        // Delete assignment operator
        auto operator=(const memory_map&) -> memory_map& = delete;

        // Allow move semantics
        memory_map(memory_map&& other) noexcept;

        // Allow move semantics
        auto operator=(memory_map&& other) noexcept -> memory_map&;

        /// Open a file for memory mapping
        /// \param path Path to the file
        /// \param mode Opening mode (ReadOnly or ReadWrite)
        /// \return true if successful, false otherwise
        [[nodiscard]] auto open(const std::string& path, Mode mode) -> bool;

        /// Close the memory mapped file and release resources
        void close();

        /// Check if the file is currently open
        /// \return true if open, false otherwise
        [[nodiscard]] auto is_open() const -> bool;

        /// Get the size of the mapped file
        /// \return Size in bytes, or 0 if not open
        [[nodiscard]] auto size() const -> std::size_t;

        /// Read data from the file at a specific offset (random access)
        /// \param offset Offset in the file to read from
        /// \param buffer Buffer to read data into
        /// \param length Number of bytes to read
        /// \return Number of bytes actually read, or 0 on error
        [[nodiscard]] auto read(std::size_t offset, void* buffer, std::size_t length) -> std::size_t;

        /// Write data sequentially to the file
        /// \param data Data to write
        /// \param length Number of bytes to write
        /// \return Number of bytes actually written, or 0 on error
        /*[[nodiscard]]*/ auto write(const void* data, std::size_t length) -> std::size_t;

        /// Flush any pending writes to disk
        /// \return true if successful, false otherwise
        [[nodiscard]] auto flush() -> bool;

        /// Get the current write position (for sequential writes)
        /// \return Current write offset in bytes
        [[nodiscard]] auto write_position() const -> std::size_t;

        /// Get a pointer to the mapped memory
        /// \return Pointer to the mapped memory, or nullptr if not open or not mapped
        [[nodiscard]] auto data() const -> const std::uint8_t*;

    private:
        void cleanup();

    private:
        /// Platform-specific implementation data
        struct Impl;
        Impl* impl_{};
    };
} // namespace pak
