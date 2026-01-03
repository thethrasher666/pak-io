//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "memory-map.hxx"

#define WIN32_LEAN_AND_MEAN
#include <cstring>
#include <windows.h>

namespace pak
{
    struct memory_map::Impl
    {
        HANDLE file_handle_ = INVALID_HANDLE_VALUE;    // File handle
        HANDLE mapping_handle_ = INVALID_HANDLE_VALUE; // File mapping handle
        void* mapped_data_{};                          // Pointer to mapped memory
        std::size_t file_size_{};                      // Size of the file
        std::size_t write_pos_{};                      // Current write position
        Mode mode_ = Mode::read_only;                  // Opening mode
        std::string filepath_;                         // Path to the file
    };

    memory_map::memory_map() : impl_(new Impl())
    {
    }

    memory_map::~memory_map()
    {
        cleanup();
        delete impl_;
    }

    memory_map::memory_map(memory_map&& other) noexcept : impl_(other.impl_)
    {
        other.impl_ = nullptr;
    }

    memory_map& memory_map::operator=(memory_map&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            delete impl_;
            impl_ = other.impl_;
            other.impl_ = nullptr;
        }
        return *this;
    }

    auto memory_map::open(const std::string& path, Mode mode) -> bool
    {
        if (is_open())
        {
            close();
        }

        impl_->filepath_ = path;
        impl_->mode_ = mode;

        // Open file with appropriate flags
        DWORD access = (mode == Mode::read_only) ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
        DWORD shareMode = FILE_SHARE_READ;
        DWORD creation = (mode == Mode::read_only) ? OPEN_EXISTING : OPEN_ALWAYS;

        impl_->file_handle_ = CreateFileA(path.c_str(), access, shareMode, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (impl_->file_handle_ == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        // Get file size
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(impl_->file_handle_, &fileSize))
        {
            CloseHandle(impl_->file_handle_);
            impl_->file_handle_ = INVALID_HANDLE_VALUE;
            return false;
        }

        impl_->file_size_ = static_cast<std::size_t>(fileSize.QuadPart);

        // For read_write mode on new files, start with size 0
        if (mode == Mode::read_write && impl_->file_size_ == 0)
        {
            // File is empty, we'll grow it as needed during writes
            return true;
        }

        // Create file mapping if file has content
        if (impl_->file_size_ > 0)
        {
            DWORD protect = (mode == Mode::read_only) ? PAGE_READONLY : PAGE_READWRITE;

            impl_->mapping_handle_ = CreateFileMappingA(impl_->file_handle_,
                                                        nullptr,
                                                        protect,
                                                        0,
                                                        0, // Use current file size
                                                        nullptr);

            if (impl_->mapping_handle_ == nullptr)
            {
                CloseHandle(impl_->file_handle_);
                impl_->file_handle_ = INVALID_HANDLE_VALUE;
                return false;
            }

            // Map view of file
            DWORD mapAccess = (mode == Mode::read_only) ? FILE_MAP_READ : FILE_MAP_WRITE;

            impl_->mapped_data_ = MapViewOfFile(impl_->mapping_handle_,
                                                mapAccess,
                                                0,
                                                0,
                                                0 // Map entire file
            );

            if (impl_->mapped_data_ == nullptr)
            {
                CloseHandle(impl_->mapping_handle_);
                impl_->mapping_handle_ = INVALID_HANDLE_VALUE;
                CloseHandle(impl_->file_handle_);
                impl_->file_handle_ = INVALID_HANDLE_VALUE;
                return false;
            }
        }

        return true;
    }

    void memory_map::close()
    {
        cleanup();
    }

    void memory_map::cleanup()
    {
        if (impl_ == nullptr)
        {
            return;
        }

        // Unmap view
        if (impl_->mapped_data_ != nullptr)
        {
            UnmapViewOfFile(impl_->mapped_data_);
            impl_->mapped_data_ = nullptr;
        }

        // Close mapping handle
        if (impl_->mapping_handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(impl_->mapping_handle_);
            impl_->mapping_handle_ = INVALID_HANDLE_VALUE;
        }

        // Close file handle
        if (impl_->file_handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(impl_->file_handle_);
            impl_->file_handle_ = INVALID_HANDLE_VALUE;
        }

        impl_->file_size_ = 0;
        impl_->write_pos_ = 0;
    }

    auto memory_map::is_open() const -> bool
    {
        return impl_ != nullptr && impl_->file_handle_ != INVALID_HANDLE_VALUE;
    }

    auto memory_map::size() const -> std::size_t
    {
        return (impl_ != nullptr) ? impl_->file_size_ : 0;
    }

    auto memory_map::read(std::size_t offset, void* buffer, std::size_t length) -> std::size_t
    {
        if (!is_open() || buffer == nullptr || length == 0)
        {
            return 0;
        }

        // Check bounds
        if (offset >= impl_->file_size_)
        {
            return 0;
        }

        // Adjust length if it exceeds file size
        std::size_t bytes_to_read = length;
        if (offset + bytes_to_read > impl_->file_size_)
        {
            bytes_to_read = impl_->file_size_ - offset;
        }

        // For empty files or unmapped data, read directly
        if (impl_->mapped_data_ == nullptr)
        {
            LARGE_INTEGER li;
            li.QuadPart = offset;
            if (!SetFilePointerEx(impl_->file_handle_, li, nullptr, FILE_BEGIN))
            {
                return 0;
            }

            DWORD bytes_read = 0;
            if (!ReadFile(impl_->file_handle_, buffer, static_cast<DWORD>(bytes_to_read), &bytes_read, nullptr))
            {
                return 0;
            }
            return static_cast<std::size_t>(bytes_read);
        }

        // Copy from mapped memory
        std::memcpy(buffer, static_cast<char*>(impl_->mapped_data_) + offset, bytes_to_read);
        return bytes_to_read;
    }

    auto memory_map::write(const void* data, std::size_t length) -> std::size_t
    {
        if (!is_open() || impl_->mode_ == Mode::read_only || data == nullptr || length == 0)
        {
            return 0;
        }

        std::size_t new_size = impl_->write_pos_ + length;

        // Grow file if necessary
        if (new_size > impl_->file_size_)
        {
            // Unmap current view
            if (impl_->mapped_data_ != nullptr)
            {
                UnmapViewOfFile(impl_->mapped_data_);
                impl_->mapped_data_ = nullptr;
            }

            // Close current mapping
            if (impl_->mapping_handle_ != INVALID_HANDLE_VALUE)
            {
                CloseHandle(impl_->mapping_handle_);
                impl_->mapping_handle_ = INVALID_HANDLE_VALUE;
            }

            // Extend file
            LARGE_INTEGER li;
            li.QuadPart = new_size;
            if (!SetFilePointerEx(impl_->file_handle_, li, nullptr, FILE_BEGIN))
            {
                return 0;
            }
            if (!SetEndOfFile(impl_->file_handle_))
            {
                return 0;
            }

            impl_->file_size_ = new_size;

            // Create new mapping
            impl_->mapping_handle_ = CreateFileMappingA(impl_->file_handle_, nullptr, PAGE_READWRITE, 0, 0, nullptr);

            if (impl_->mapping_handle_ == nullptr)
            {
                return 0;
            }

            // Map new view
            impl_->mapped_data_ = MapViewOfFile(impl_->mapping_handle_, FILE_MAP_WRITE, 0, 0, 0);

            if (impl_->mapped_data_ == nullptr)
            {
                CloseHandle(impl_->mapping_handle_);
                impl_->mapping_handle_ = INVALID_HANDLE_VALUE;
                return 0;
            }
        }

        // Write to mapped memory
        std::memcpy(static_cast<char*>(impl_->mapped_data_) + impl_->write_pos_, data, length);
        impl_->write_pos_ += length;

        return length;
    }

    auto memory_map::flush() -> bool
    {
        if (!is_open() || impl_->mapped_data_ == nullptr)
        {
            return false;
        }

        // Flush mapped memory to disk
        return FlushViewOfFile(impl_->mapped_data_, 0) != 0;
    }

    auto memory_map::write_position() const -> std::size_t
    {
        return (impl_ != nullptr) ? impl_->write_pos_ : 0;
    }

    auto memory_map::data() const -> const std::uint8_t*
    {
        if (!is_open() || impl_->mapped_data_ == nullptr)
        {
            return nullptr;
        }
        return static_cast<const std::uint8_t*>(impl_->mapped_data_);
    }

} // namespace pak
