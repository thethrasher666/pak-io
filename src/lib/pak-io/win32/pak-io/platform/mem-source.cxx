//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/platform/mem-source.hxx"

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace pk
{
    namespace
    {
        auto lastErrorWin32() -> std::error_code
        {
            return std::error_code(static_cast<int>(::GetLastError()), std::system_category());
        }
    } // namespace

    MemSourcePlatform::~MemSourcePlatform()
    {
        close();
    }

    auto MemSourcePlatform::open(std::filesystem::path const& path) -> std::expected<void, std::error_code>
    {
        close();

        if (_file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); _file == INVALID_HANDLE_VALUE)
        {
            return std::unexpected(lastErrorWin32());
        }

        auto const fail = [this](std::error_code const error) -> std::expected<void, std::error_code>
        {
            close();
            return std::unexpected(error);
        };

        LARGE_INTEGER fileSize{};
        if (!::GetFileSizeEx(_file, &fileSize))
        {
            return fail(lastErrorWin32());
        }

        if (fileSize.QuadPart < 0 || static_cast<std::uint64_t>(fileSize.QuadPart) > std::numeric_limits<std::size_t>::max())
        {
            return fail(std::make_error_code(std::errc::value_too_large));
        }

        _size = static_cast<std::size_t>(fileSize.QuadPart);

        // Windows does not permit a zero-length file mapping.
        if (_size == 0)
        {
            return {};
        }

        _mapping = ::CreateFileMappingW(_file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!_mapping)
        {
            return fail(lastErrorWin32());
        }

        _view = ::MapViewOfFile(_mapping, FILE_MAP_READ, 0, 0, _size);
        if (!_view)
        {
            return fail(lastErrorWin32());
        }

        return {};
    }

    void MemSourcePlatform::close()
    {
        if (_view)
        {
            ::UnmapViewOfFile(_view);
            _view = nullptr;
        }

        if (_mapping)
        {
            ::CloseHandle(_mapping);
            _mapping = nullptr;
        }

        if (_file != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(_file);
            _file = INVALID_HANDLE_VALUE;
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
