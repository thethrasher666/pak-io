//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io.hxx"

#include <cstring>
#include <fstream>
#include <lz4.h>

namespace pak
{
    // Endianness conversion helpers - ensure consistent file format across platforms
    namespace
    {
        auto to_little_endian_32(std::uint32_t value) -> std::uint32_t
        {
            // Check if we're on a little-endian system
            constexpr std::uint32_t test = 1;
            const bool is_little_endian = (*reinterpret_cast<const std::uint8_t*>(&test) == 1);

            if (is_little_endian)
            {
                return value;
            }
            else
            {
                // Swap bytes for big-endian systems
                return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
            }
        }

        auto to_little_endian_64(std::uint64_t value) -> std::uint64_t
        {
            constexpr std::uint32_t test = 1;
            const bool is_little_endian = (*reinterpret_cast<const std::uint8_t*>(&test) == 1);

            if (is_little_endian)
            {
                return value;
            }
            else
            {
                // Swap bytes for big-endian systems
                return ((value & 0x00000000000000FFULL) << 56) | ((value & 0x000000000000FF00ULL) << 40) | ((value & 0x0000000000FF0000ULL) << 24) |
                       ((value & 0x00000000FF000000ULL) << 8) | ((value & 0x000000FF00000000ULL) >> 8) | ((value & 0x0000FF0000000000ULL) >> 24) |
                       ((value & 0x00FF000000000000ULL) >> 40) | ((value & 0xFF00000000000000ULL) >> 56);
            }
        }
    } // namespace

    // Magic number for pak files: "PAKF" (Pak File)
    constexpr std::uint32_t pak_magic = 0x46504B50; // 'PAKF' in little-endian
    constexpr std::uint32_t pak_version = 1;

    pak_io::pak_io(std::size_t worker_threads)
        : memory_map_(), entries_(), data_offset_(0), read_mode_(false),
          compression_queue_(worker_threads,
                             [this](const std::string& filename, const std::vector<std::uint8_t>& data)
                             {
                                 // Compress the data
                                 std::size_t uncompressed_size = data.size();
                                 int max_compressed_size = LZ4_compressBound(static_cast<int>(uncompressed_size));
                                 std::vector<std::uint8_t> compressed_data(max_compressed_size);

                                 int compressed_size = LZ4_compress_default(reinterpret_cast<const char*>(data.data()),
                                                                            reinterpret_cast<char*>(compressed_data.data()),
                                                                            static_cast<int>(uncompressed_size),
                                                                            max_compressed_size);

                                 if (compressed_size <= 0)
                                 {
                                     // Compression failed, store uncompressed
                                     compressed_data.assign(data.begin(), data.end());
                                     compressed_size = static_cast<int>(uncompressed_size);
                                 }
                                 else
                                 {
                                     compressed_data.resize(compressed_size);
                                 }

                                 // Write compressed data to archive (synchronized)
                                 {
                                     std::lock_guard<std::mutex> lock(write_mutex_);

                                     file_entry entry;
                                     entry.name = filename;
                                     entry.offset = memory_map_.write_position();
                                     entry.compressed_size = static_cast<std::uint64_t>(compressed_size);
                                     entry.uncompressed_size = static_cast<std::uint64_t>(uncompressed_size);

                                     memory_map_.write(compressed_data.data(), compressed_size);
                                     entries_.push_back(std::move(entry));
                                 }
                             })
    {
    }

    pak_io::~pak_io()
    {
        // Destructor cannot handle return value, best effort close
        (void) close();
    }

    auto pak_io::open(const std::string& path) -> bool
    {
        if (is_open())
        {
            return close();
        }

        if (!memory_map_.open(path, memory_map::Mode::read_write))
        {
            return false;
        }

        entries_.clear();
        // Data starts immediately at offset 0 (no header at beginning)
        data_offset_ = 0;
        read_mode_ = false;

        return true;
    }

    auto pak_io::add_file(const std::string& identifier, const std::string& source_path) -> bool
    {
        if (!is_open() || identifier.empty() || source_path.empty())
        {
            return false;
        }

        // Read file data from disk
        std::ifstream file(source_path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> file_data(size);
        if (!file.read(reinterpret_cast<char*>(file_data.data()), size))
        {
            return false;
        }

        // Enqueue compression job with identifier
        compression_queue_.enqueue(identifier, std::move(file_data));

        return true;
    }

    auto pak_io::close() -> bool
    {
        if (!is_open())
        {
            return true;
        }

        // If opened for reading, just close without writing
        if (read_mode_)
        {
            memory_map_.close();
            entries_.clear();
            data_offset_ = 0;
            read_mode_ = false;
            return true;
        }

        // Wait for all compression jobs to complete
        compression_queue_.wait_for_completion();

        // Write the directory (file table) after all file data
        std::uint64_t table_offset = memory_map_.write_position();

        // Write each entry: name_length(4) + name + offset(8) + compressed_size(8) + uncompressed_size(8)
        for (const auto& entry : entries_)
        {
            std::uint32_t name_length = to_little_endian_32(static_cast<std::uint32_t>(entry.name.size()));
            std::uint64_t offset = to_little_endian_64(entry.offset);
            std::uint64_t compressed_size = to_little_endian_64(entry.compressed_size);
            std::uint64_t uncompressed_size = to_little_endian_64(entry.uncompressed_size);

            memory_map_.write(&name_length, sizeof(name_length));
            memory_map_.write(entry.name.data(), entry.name.size());
            memory_map_.write(&offset, sizeof(offset));
            memory_map_.write(&compressed_size, sizeof(compressed_size));
            memory_map_.write(&uncompressed_size, sizeof(uncompressed_size));
        }

        // Write footer at the very end with directory location
        // Footer structure: magic(4) + version(4) + entry_count(8) + table_offset(8) = 24 bytes
        // When opening for reading, seek to end minus 24 bytes to quickly find the directory
        std::uint32_t magic = to_little_endian_32(pak_magic);
        std::uint32_t version = to_little_endian_32(pak_version);
        std::uint64_t entry_count = to_little_endian_64(static_cast<std::uint64_t>(entries_.size()));
        std::uint64_t table_offset_le = to_little_endian_64(table_offset);

        memory_map_.write(&magic, sizeof(magic));
        memory_map_.write(&version, sizeof(version));
        memory_map_.write(&entry_count, sizeof(entry_count));
        memory_map_.write(&table_offset_le, sizeof(table_offset_le));

        bool flush_result = memory_map_.flush();
        memory_map_.close();
        entries_.clear();
        data_offset_ = 0;
        read_mode_ = false;

        return flush_result;
    }

    auto pak_io::is_open() const -> bool
    {
        return memory_map_.is_open();
    }

    auto pak_io::open_for_reading(const std::string& path) -> bool
    {
        if (is_open())
        {
            (void) close();
        }

        if (!memory_map_.open(path, memory_map::Mode::read_only))
        {
            return false;
        }

        entries_.clear();
        read_mode_ = true;

        // Read footer (last 24 bytes)
        std::size_t file_size = memory_map_.size();
        if (file_size < 24)
        {
            memory_map_.close();
            read_mode_ = false;
            return false;
        }

        const std::uint8_t* file_data = memory_map_.data();
        if (!file_data)
        {
            memory_map_.close();
            read_mode_ = false;
            return false;
        }

        std::uint64_t footer_offset = file_size - 24;
        const std::uint8_t* footer_data = file_data + footer_offset;

        // Read footer fields
        std::uint32_t magic;
        std::uint32_t version;
        std::uint64_t entry_count;
        std::uint64_t table_offset;

        std::memcpy(&magic, footer_data, sizeof(magic));
        std::memcpy(&version, footer_data + 4, sizeof(version));
        std::memcpy(&entry_count, footer_data + 8, sizeof(entry_count));
        std::memcpy(&table_offset, footer_data + 16, sizeof(table_offset));

        // Convert from little-endian
        magic = to_little_endian_32(magic);
        version = to_little_endian_32(version);
        entry_count = to_little_endian_64(entry_count);
        table_offset = to_little_endian_64(table_offset);

        // Validate magic and version
        if (magic != pak_magic || version != pak_version)
        {
            memory_map_.close();
            read_mode_ = false;
            return false;
        }

        // Read directory entries
        const std::uint8_t* table_data = file_data + table_offset;
        std::size_t table_position = 0;

        for (std::uint64_t i = 0; i < entry_count; ++i)
        {
            file_entry entry;

            // Read name_length
            std::uint32_t name_length;
            std::memcpy(&name_length, table_data + table_position, sizeof(name_length));
            name_length = to_little_endian_32(name_length);
            table_position += sizeof(name_length);

            // Read name
            entry.name.assign(reinterpret_cast<const char*>(table_data + table_position), name_length);
            table_position += name_length;

            // Read offset, compressed_size, uncompressed_size
            std::memcpy(&entry.offset, table_data + table_position, sizeof(entry.offset));
            table_position += sizeof(entry.offset);
            std::memcpy(&entry.compressed_size, table_data + table_position, sizeof(entry.compressed_size));
            table_position += sizeof(entry.compressed_size);
            std::memcpy(&entry.uncompressed_size, table_data + table_position, sizeof(entry.uncompressed_size));
            table_position += sizeof(entry.uncompressed_size);

            // Convert from little-endian
            entry.offset = to_little_endian_64(entry.offset);
            entry.compressed_size = to_little_endian_64(entry.compressed_size);
            entry.uncompressed_size = to_little_endian_64(entry.uncompressed_size);

            entries_.push_back(std::move(entry));
        }

        return true;
    }

    auto pak_io::list_files() const -> std::vector<std::string>
    {
        std::vector<std::string> file_list;
        file_list.reserve(entries_.size());
        for (const auto& entry : entries_)
        {
            file_list.push_back(entry.name);
        }
        return file_list;
    }

    auto pak_io::get_file_info() const -> std::vector<file_info>
    {
        std::vector<file_info> info_list;
        info_list.reserve(entries_.size());
        for (const auto& entry : entries_)
        {
            file_info info;
            info.name = entry.name;
            info.compressed_size = entry.compressed_size;
            info.uncompressed_size = entry.uncompressed_size;
            info.offset = entry.offset;
            info_list.push_back(std::move(info));
        }
        return info_list;
    }

    auto pak_io::extract_file(const std::string& identifier, const std::string& destination_path) -> bool
    {
        if (!read_mode_ || !is_open())
        {
            return false;
        }

        // Find the entry
        const file_entry* entry = nullptr;
        for (const auto& e : entries_)
        {
            if (e.name == identifier)
            {
                entry = &e;
                break;
            }
        }

        if (!entry)
        {
            return false;
        }

        // Check if memory is mapped
        const std::uint8_t* base_data = memory_map_.data();
        if (!base_data)
        {
            return false;
        }

        // Read compressed data
        const std::uint8_t* compressed_data = base_data + entry->offset;

        // Decompress if needed
        std::vector<std::uint8_t> uncompressed_data(entry->uncompressed_size);

        if (entry->compressed_size == entry->uncompressed_size)
        {
            // Data was stored uncompressed
            std::memcpy(uncompressed_data.data(), compressed_data, entry->uncompressed_size);
        }
        else
        {
            // Decompress with LZ4
            int result = LZ4_decompress_safe(reinterpret_cast<const char*>(compressed_data),
                                            reinterpret_cast<char*>(uncompressed_data.data()),
                                            static_cast<int>(entry->compressed_size),
                                            static_cast<int>(entry->uncompressed_size));

            if (result < 0 || static_cast<std::uint64_t>(result) != entry->uncompressed_size)
            {
                return false;
            }
        }

        // Write to destination file
        std::ofstream out_file(destination_path, std::ios::binary);
        if (!out_file.is_open())
        {
            return false;
        }

        out_file.write(reinterpret_cast<const char*>(uncompressed_data.data()), entry->uncompressed_size);
        return out_file.good();
    }

} // namespace pak
