//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/platform/mem-sink.hxx"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace pk
{
    namespace
    {
        constexpr std::size_t initialCapacity = 64u * 1024u;

        auto lastErrorWin32() -> std::error_code
        {
            return std::error_code(static_cast<int>(::GetLastError()), std::system_category());
        }
    } // namespace

    MemSinkPlatform::~MemSinkPlatform()
    {
        close();
    }

    auto MemSinkPlatform::open(std::filesystem::path const& path) -> std::expected<void, std::error_code>
    {
        close();

        if (_file = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            _file == INVALID_HANDLE_VALUE)
        {
            return std::unexpected(lastErrorWin32());
        }

        SYSTEM_INFO systemInfo{};
        ::GetSystemInfo(&systemInfo);
        _allocationGranularity = systemInfo.dwAllocationGranularity;

        return resizeAndMap(initialCapacity);
    }

    void MemSinkPlatform::close()
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
            LARGE_INTEGER end{};
            end.QuadPart = static_cast<LONGLONG>(_position);
            if (::SetFilePointerEx(_file, end, nullptr, FILE_BEGIN))
            {
                ::SetEndOfFile(_file);
            }
            ::CloseHandle(_file);
            _file = INVALID_HANDLE_VALUE;
        }

        _capacity = 0;
        _position = 0;
        _allocationGranularity = 0;
    }

    auto MemSinkPlatform::resizeAndMap(std::size_t size) -> std::expected<void, std::error_code>
    {
        auto const granularity = static_cast<std::size_t>(_allocationGranularity);
        if (granularity == 0 || size > std::numeric_limits<std::size_t>::max() - granularity + 1)
        {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }
        size = ((size + granularity - 1) / granularity) * granularity;

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

        LARGE_INTEGER fileSize{};
        fileSize.QuadPart = static_cast<LONGLONG>(size);
        if (!::SetFilePointerEx(_file, fileSize, nullptr, FILE_BEGIN) || !::SetEndOfFile(_file))
        {
            return std::unexpected(lastErrorWin32());
        }

        _mapping = ::CreateFileMappingW(_file, nullptr, PAGE_READWRITE, static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), nullptr);
        if (!_mapping)
        {
            return std::unexpected(lastErrorWin32());
        }

        _view = ::MapViewOfFile(_mapping, FILE_MAP_WRITE, 0, 0, size);
        if (!_view)
        {
            auto const error = lastErrorWin32();
            ::CloseHandle(_mapping);
            _mapping = nullptr;
            return std::unexpected(error);
        }

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
