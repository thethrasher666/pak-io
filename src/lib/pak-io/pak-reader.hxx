//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace pk
{
    class MemSourcePlatform;

    /// A single entry within a PAK file.
    struct PakEntry
    {
        std::string    identifier; ///< The identifier (filename stem) of the entry.
        std::string    filename;   ///< The original filename of the entry, as supplied in the manifest (including its extension).
        std::size_t    size{};     ///< The uncompressed size of the entry, in bytes.
        uint8_t const* data{};     ///< Pointer to the entry's payload, in memory. This is only valid while the PAK file is open.
    };

    /// A class that provides read-only access to a PAK file, using memory-mapped I/O.
    class PakReader final
    {
    public:
        /// Constructor.
        PakReader();

        /// Destructor.
        ~PakReader();

        /// Closes the PAK file.
        void close();

        /// Opens a PAK file for reading.
        /// \param path The path to the PAK file to open.
        /// \return void if the PAK file was opened successfully, an error otherwise.
        [[nodiscard]] auto open(std::filesystem::path const& path) -> std::expected<void, std::error_code>;

        /// \return The name of the PAK file.
        [[nodiscard]] auto name() const noexcept -> std::string const&;

        /// \return The entries within the PAK file, in the order they appear in the file.
        [[nodiscard]] auto entries() const noexcept -> std::span<PakEntry const>;

        /// Finds an entry by its identifier.
        /// \param identifier The identifier (filename stem) to look for.
        /// \return The entry, or nullptr if no entry has that identifier.
        [[nodiscard]] auto find(std::string_view identifier) const -> PakEntry const*;

        /// \return The size of the PAK file, in bytes.
        [[nodiscard]] auto size() const -> std::size_t;

        /// \return The current read position, in bytes from the start of the file.
        [[nodiscard]] auto position() const -> std::size_t;

        /// Moves the read position.
        /// \param position The new read position, in bytes from the start of the file.
        void seek(std::size_t position);

        /// \return A read-only view of the whole PAK file.
        [[nodiscard]] auto view() const -> std::span<std::byte const>;

        /// Read an arbitrary amount of data from the current position, advancing it.
        /// \param data The buffer to read the data into.
        /// \param size The size of the buffer.
        void read(void* data, std::size_t size);

        /// Read an arbitrary amount of data from an absolute offset, leaving the current position untouched.
        /// \param data The buffer to read the data into.
        /// \param size The size of the buffer.
        /// \param offset The offset to read from, in bytes from the start of the file.
        void readAt(void* data, std::size_t size, std::size_t offset) const;

        /// Read the contents of a container-like object from the memory-mapped file.
        template <typename T>
        void read(std::vector<T>& data)
        {
            read(data.data(), data.size() * sizeof(T));
        }

        /// Read the contents of a container-like object from an absolute offset in the memory-mapped file.
        template <typename T>
        void readAt(std::vector<T>& data, std::size_t offset) const
        {
            readAt(data.data(), data.size() * sizeof(T), offset);
        }

    private:
        /// Reads and validates the header, entry table and string table of the mapped file.
        /// \return void if the PAK file was parsed successfully, an error otherwise.
        [[nodiscard]] auto parse() -> std::expected<void, std::error_code>;

    private:
        std::vector<PakEntry>              _entries;   ///< The entries within the PAK file.
        std::unique_ptr<MemSourcePlatform> _memSource; ///< The platform-specific implementation of the PAK source.
        std::string                        _name;      ///< The name of the PAK file.
    };
} // namespace pk
