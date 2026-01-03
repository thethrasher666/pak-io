//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include "pak-io/compression-queue/compression-queue.hxx"
#include "pak-io/memory-map/memory-map.hxx"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace pak
{
    /// pak_io provides the capabilities to read and write compressed compound archives of "hero" files.
    class pak_io
    {
    public:
        /// Constructor
        /// \param worker_threads Number of compression worker threads (0 = hardware concurrency)
        explicit pak_io(std::size_t worker_threads = 0);

        /// Destructor
        ~pak_io();

        /// Delete copy constructor
        pak_io(const pak_io&) = delete;

        /// Delete assignment operator
        auto operator=(const pak_io&) -> pak_io& = delete;

        /// Delete move constructor (contains non-movable compression_queue)
        pak_io(pak_io&& other) = delete;

        /// Delete move assignment (contains non-movable compression_queue)
        auto operator=(pak_io&& other) -> pak_io& = delete;

        /// Open a pak file for writing
        /// \param path Path to the pak file to create
        /// \return true if successful, false otherwise
        /// \note File format: [File Data...][Directory][Footer]
        /// \note Footer is fixed 24 bytes at end containing directory offset for fast reading
        [[nodiscard]] auto open(const std::string& path) -> bool;

        /// Add a file to the archive
        /// \param identifier Identifier for the file in the archive (use forward slashes for paths)
        /// \param source_path Absolute path to the file on disk to read and compress
        /// \return true if successful, false otherwise
        [[nodiscard]] auto add_file(const std::string& identifier, const std::string& source_path) -> bool;

        /// Finish writing and close the pak file
        /// \return true if successful, false otherwise
        [[nodiscard]] auto close() -> bool;

        /// Check if the pak file is currently open
        /// \return true if open, false otherwise
        [[nodiscard]] auto is_open() const -> bool;

        /// Open a pak file for reading
        /// \param path Path to the pak file to read
        /// \return true if successful, false otherwise
        [[nodiscard]] auto open_for_reading(const std::string& path) -> bool;

        /// Get list of file identifiers in the archive
        /// \return vector of file identifiers
        [[nodiscard]] auto list_files() const -> std::vector<std::string>;

        /// File information structure
        struct file_info
        {
            std::string name;
            std::uint64_t compressed_size;
            std::uint64_t uncompressed_size;
            std::uint64_t offset;
        };

        /// Get detailed information about all files in the archive
        /// \return vector of file information structures
        [[nodiscard]] auto get_file_info() const -> std::vector<file_info>;

        /// Extract a file from the archive
        /// \param identifier Identifier of the file in the archive
        /// \param destination_path Path where the extracted file should be written
        /// \return true if successful, false otherwise
        [[nodiscard]] auto extract_file(const std::string& identifier, const std::string& destination_path) -> bool;

    private:
        void write_header();

    private:
        struct file_entry
        {
            std::string name;
            std::uint64_t offset;
            std::uint64_t compressed_size;
            std::uint64_t uncompressed_size;
        };

        memory_map memory_map_;
        std::vector<file_entry> entries_;
        std::uint64_t data_offset_;
        bool read_mode_;

        // Compression queue
        compression_queue compression_queue_;

        // Write synchronization
        std::mutex write_mutex_;
    };
} // namespace pak
