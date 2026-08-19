//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/platform/mem-source.hxx"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pk
{
    namespace
    {
        auto lastErrorPosix() -> std::error_code
        {
            return std::error_code(errno, std::generic_category());
        }
    } // namespace

    MemSourcePlatform::~MemSourcePlatform()
    {
        close();
    }

    auto MemSourcePlatform::open(std::filesystem::path const& path) -> std::expected<void, std::error_code>
    {
        close();

        if (_file = ::open(path.c_str(), O_RDONLY); _file == -1)
        {
            return std::unexpected(lastErrorPosix());
        }

        auto const fail = [this](std::error_code const error)
        {
            ::close(_file);
            _file = -1;
            return std::unexpected(error);
        };

        struct stat info
        {
        };
        if (::fstat(_file, &info) == -1)
        {
            return fail(lastErrorPosix());
        }

        // Only regular files have a size that can be mapped in its entirety.
        if (!S_ISREG(info.st_mode))
        {
            return fail(std::make_error_code(std::errc::invalid_argument));
        }

        auto const fileSize = static_cast<std::uintmax_t>(info.st_size);
        if (fileSize > std::numeric_limits<std::size_t>::max())
        {
            return fail(std::make_error_code(std::errc::value_too_large));
        }
        _size = static_cast<std::size_t>(fileSize);

        // mmap rejects zero-length mappings, so an empty source simply has no view.
        if (_size == 0)
        {
            return {};
        }

        auto* const view = ::mmap(nullptr, _size, PROT_READ, MAP_PRIVATE, _file, 0);
        if (view == MAP_FAILED)
        {
            auto const error = lastErrorPosix();
            _size = 0;
            return fail(error);
        }

        _view = view;
        return {};
    }

    void MemSourcePlatform::close()
    {
        if (_view)
        {
            ::munmap(_view, _size);
            _view = nullptr;
        }

        if (_file != -1)
        {
            ::close(_file);
            _file = -1;
        }

        _size = 0;
        _position = 0;
    }

    auto MemSourcePlatform::size() const noexcept -> std::size_t
    {
        return _size;
    }

    auto MemSourcePlatform::position() const noexcept -> std::size_t
    {
        return _position;
    }

    void MemSourcePlatform::seek(std::size_t position)
    {
        if (position > _size)
        {
            throw std::out_of_range("seek beyond the end of the memory-mapped source");
        }
        _position = position;
    }

    auto MemSourcePlatform::view() const noexcept -> std::span<std::byte const>
    {
        return { static_cast<std::byte const*>(_view), _size };
    }

    void MemSourcePlatform::read(void* data, std::size_t size)
    {
        readAt(data, size, _position);
        _position += size;
    }

    void MemSourcePlatform::readAt(void* data, std::size_t size, std::size_t offset) const
    {
        if (size == 0)
        {
            return;
        }
        if (offset > _size || size > _size - offset)
        {
            throw std::out_of_range("read beyond the end of the memory-mapped source");
        }

        std::memcpy(data, static_cast<std::byte const*>(_view) + offset, size);
    }
} // namespace pk
