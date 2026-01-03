//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "memory-map.hxx"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pak
{
    struct memory_map::Impl
    {
        int fd_ = -1;                 // File descriptor
        void* mapped_data_ = nullptr; // Pointer to mapped memory
        std::size_t file_size_ = 0;   // Size of the file
        std::size_t write_pos_ = 0;   // Current write position
        Mode mode_ = Mode::read_only; // Opening mode
        std::string filepath_;        // Path to the file
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
        int flags = (mode == Mode::read_only) ? O_RDONLY : (O_RDWR | O_CREAT);
        int permissions = 0644;

        impl_->fd_ = ::open(path.c_str(), flags, permissions);
        if (impl_->fd_ == -1)
        {
            return false;
        }

        // Get file size
        struct stat st;
        if (fstat(impl_->fd_, &st) == -1)
        {
            ::close(impl_->fd_);
            impl_->fd_ = -1;
            return false;
        }

        impl_->file_size_ = static_cast<std::size_t>(st.st_size);

        // For read_write mode on new files, start with size 0
        if (mode == Mode::read_write && impl_->file_size_ == 0)
        {
            // File is empty, we'll grow it as needed during writes
            return true;
        }

        // Map the file into memory if it has content
        if (impl_->file_size_ > 0)
        {
            int prot = (mode == Mode::read_only) ? PROT_READ : (PROT_READ | PROT_WRITE);
            int mapFlags = MAP_SHARED;

            impl_->mapped_data_ = ::mmap(nullptr, impl_->file_size_, prot, mapFlags, impl_->fd_, 0);
            if (impl_->mapped_data_ == MAP_FAILED)
            {
                impl_->mapped_data_ = nullptr;
                ::close(impl_->fd_);
                impl_->fd_ = -1;
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

        // Unmap memory
        if (impl_->mapped_data_ != nullptr)
        {
            ::munmap(impl_->mapped_data_, impl_->file_size_);
            impl_->mapped_data_ = nullptr;
        }

        // Close file descriptor
        if (impl_->fd_ != -1)
        {
            ::close(impl_->fd_);
            impl_->fd_ = -1;
        }

        impl_->file_size_ = 0;
        impl_->write_pos_ = 0;
    }

    auto memory_map::is_open() const -> bool
    {
        return impl_ != nullptr && impl_->fd_ != -1;
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
            if (::lseek(impl_->fd_, offset, SEEK_SET) == -1)
            {
                return 0;
            }
            ssize_t bytes_read = ::read(impl_->fd_, buffer, bytes_to_read);
            return (bytes_read > 0) ? static_cast<std::size_t>(bytes_read) : 0;
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
            // Unmap current mapping
            if (impl_->mapped_data_ != nullptr)
            {
                ::munmap(impl_->mapped_data_, impl_->file_size_);
                impl_->mapped_data_ = nullptr;
            }

            // Extend file
            if (ftruncate(impl_->fd_, new_size) == -1)
            {
                return 0;
            }

            // Remap with new size
            impl_->file_size_ = new_size;
            impl_->mapped_data_ = ::mmap(nullptr, impl_->file_size_, PROT_READ | PROT_WRITE, MAP_SHARED, impl_->fd_, 0);
            if (impl_->mapped_data_ == MAP_FAILED)
            {
                impl_->mapped_data_ = nullptr;
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
        if (!is_open())
        {
            return false;
        }

        // If no mapped data, just sync the file descriptor
        if (impl_->mapped_data_ == nullptr)
        {
            return ::fsync(impl_->fd_) == 0;
        }

        // Sync mapped memory to disk
        return ::msync(impl_->mapped_data_, impl_->file_size_, MS_SYNC) == 0;
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
