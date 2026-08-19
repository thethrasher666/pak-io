//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/platform/mem-sink.hxx"

#include <algorithm>
#include <cerrno>
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
        constexpr std::size_t initialCapacity = 64u * 1024u;

        auto lastErrorPosix() -> std::error_code
        {
            return std::error_code(errno, std::generic_category());
        }
    } // namespace

    MemSinkPlatform::~MemSinkPlatform()
    {
        close();
    }

    auto MemSinkPlatform::open(std::filesystem::path const& path) -> std::expected<void, std::error_code>
    {
        close();

        if (_file = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); _file == -1)
        {
            return std::unexpected(lastErrorPosix());
        }

        auto const pageSize = ::sysconf(_SC_PAGESIZE);
        if (pageSize <= 0)
        {
            auto const error = lastErrorPosix();
            ::close(_file);
            _file = -1;
            return std::unexpected(error);
        }
        _pageSize = static_cast<std::size_t>(pageSize);

        return resizeAndMap(initialCapacity);
    }

    void MemSinkPlatform::close()
    {
        if (_view)
        {
            ::munmap(_view, _capacity);
            _view = nullptr;
        }

        if (_file != -1)
        {
            std::ignore = ::ftruncate(_file, static_cast<off_t>(_position));
            ::close(_file);
            _file = -1;
        }

        _capacity = 0;
        _position = 0;
        _pageSize = 0;
    }

    auto MemSinkPlatform::resizeAndMap(std::size_t size) -> std::expected<void, std::error_code>
    {
        if (_pageSize == 0 || size > std::numeric_limits<std::size_t>::max() - _pageSize + 1)
        {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }
        size = ((size + _pageSize - 1) / _pageSize) * _pageSize;

        if (size > static_cast<std::size_t>(std::numeric_limits<off_t>::max()))
        {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }

        if (_view)
        {
            ::munmap(_view, _capacity);
            _view = nullptr;
        }

        // Pages mapped beyond the file's end raise SIGBUS, so grow the file first.
        if (::ftruncate(_file, static_cast<off_t>(size)) == -1)
        {
            return std::unexpected(lastErrorPosix());
        }

        auto* const view = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, _file, 0);
        if (view == MAP_FAILED)
        {
            return std::unexpected(lastErrorPosix());
        }

        _view = view;
        _capacity = size;
        return {};
    }

    void MemSinkPlatform::write(void const* data, std::size_t size)
    {
        if (size == 0)
        {
            return;
        }
        if (size > std::numeric_limits<std::size_t>::max() - _position)
        {
            throw std::length_error("memory-mapped sink size overflow");
        }

        auto const required = _position + size;
        if (required > _capacity)
        {
            auto const doubledCapacity = _capacity <= std::numeric_limits<std::size_t>::max() / 2 ? _capacity * 2 : _capacity;
            auto const newCapacity = std::max(required, doubledCapacity);
            if (auto const result = resizeAndMap(newCapacity); !result)
            {
                throw std::system_error(result.error(), "failed to resize memory-mapped sink");
            }
        }

        std::memcpy(static_cast<std::byte*>(_view) + _position, data, size);
        _position = required;
    }
} // namespace pk
