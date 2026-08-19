//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-reader.hxx"
#include "pak-io/pak-error.hxx"
#include "pak-io/platform/mem-source.hxx"
#include "pak-io/pod.hxx"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>

namespace pk
{
    namespace
    {
        /// Reads a NUL-terminated string from the string table.
        /// \param view The mapped file.
        /// \param offset The offset of the string, from the start of the file.
        /// \return The string, or nullopt if the offset is out of range or the string is unterminated.
        [[nodiscard]] auto readString(std::span<std::byte const> const view, std::uint64_t const offset) -> std::optional<std::string>
        {
            if (offset >= view.size())
            {
                return std::nullopt;
            }

            auto const* const begin = reinterpret_cast<char const*>(view.data()) + offset;
            auto const* const terminator = static_cast<char const*>(std::memchr(begin, '\0', view.size() - offset));
            if (terminator == nullptr)
            {
                return std::nullopt;
            }

            return std::string(begin, terminator);
        }

        /// \return True if [offset, offset + length) lies wholly within the mapped file.
        [[nodiscard]] auto contains(std::span<std::byte const> const view, std::uint64_t const offset, std::uint64_t const length) -> bool
        {
            return offset <= view.size() && length <= view.size() - offset;
        }
    } // namespace

    PakReader::PakReader()
    {
    }

    PakReader::~PakReader()
    {
    }

    void PakReader::close()
    {
        _entries.clear();
        _name.clear();

        if (_memSource)
        {
            _memSource->close();
            _memSource.reset();
        }
    }

    auto PakReader::open(std::filesystem::path const& path) -> std::expected<void, std::error_code>
    {
        if (_memSource)
        {
            return std::unexpected(makeErrorCode(ErrorCode::FailedToOpenFileForReading));
        }

        _memSource = std::make_unique<MemSourcePlatform>();
        if (auto const result = _memSource->open(path); !result)
        {
            _memSource.reset();
            return result;
        }

        if (auto const result = parse(); !result)
        {
            close();
            return result;
        }

        return {};
    }

    auto PakReader::parse() -> std::expected<void, std::error_code>
    {
        auto const view = _memSource->view();
        if (view.size() < sizeof(PAK_FILE_HEADER))
        {
            return std::unexpected(makeErrorCode(ErrorCode::NotAPakFile));
        }

        PAK_FILE_HEADER header;
        std::memcpy(&header, view.data(), sizeof(header));

        if (std::memcmp(header.magic, PAK_FILE_HEADER_MAGIC, sizeof(header.magic)) != 0)
        {
            return std::unexpected(makeErrorCode(ErrorCode::NotAPakFile));
        }

        if (header.count > (std::numeric_limits<std::uint64_t>::max() - sizeof(PAK_FILE_HEADER)) / sizeof(PAK_ENTRY) ||
            !contains(view, sizeof(PAK_FILE_HEADER), header.count * sizeof(PAK_ENTRY)))
        {
            return std::unexpected(makeErrorCode(ErrorCode::CorruptPakFile));
        }

        auto name = readString(view, header.name);
        if (!name)
        {
            return std::unexpected(makeErrorCode(ErrorCode::CorruptPakFile));
        }

        std::vector<PakEntry> entries;
        entries.reserve(header.count);

        for (std::uint64_t index = 0; index < header.count; ++index)
        {
            PAK_ENTRY entry;
            std::memcpy(&entry, view.data() + sizeof(PAK_FILE_HEADER) + index * sizeof(PAK_ENTRY), sizeof(entry));

            auto identifier = readString(view, entry.identifier);
            auto filename = readString(view, entry.filename);
            if (!identifier || !filename || !contains(view, entry.data, entry.size))
            {
                return std::unexpected(makeErrorCode(ErrorCode::CorruptPakFile));
            }

            entries.push_back(PakEntry{ .identifier = std::move(*identifier),
                                        .filename = std::move(*filename),
                                        .size = static_cast<std::size_t>(entry.size),
                                        .data = reinterpret_cast<uint8_t const*>(view.data()) + entry.data });
        }

        _name = std::move(*name);
        _entries = std::move(entries);
        return {};
    }

    auto PakReader::name() const noexcept -> std::string const&
    {
        return _name;
    }

    auto PakReader::entries() const noexcept -> std::span<PakEntry const>
    {
        return _entries;
    }

    auto PakReader::find(std::string_view const identifier) const -> PakEntry const*
    {
        auto const entry = std::ranges::find(_entries, identifier, &PakEntry::identifier);
        return entry == _entries.end() ? nullptr : &*entry;
    }

    void PakReader::read(void* data, std::size_t size)
    {
        _memSource->read(data, size);
    }

    void PakReader::readAt(void* data, std::size_t size, std::size_t offset) const
    {
        _memSource->readAt(data, size, offset);
    }

    auto PakReader::size() const -> std::size_t
    {
        return _memSource ? _memSource->size() : 0u;
    }

    auto PakReader::position() const -> std::size_t
    {
        return _memSource ? _memSource->position() : 0u;
    }

    void PakReader::seek(std::size_t position)
    {
        _memSource->seek(position);
    }

    auto PakReader::view() const -> std::span<std::byte const>
    {
        return _memSource ? _memSource->view() : std::span<std::byte const>{};
    }
} // namespace pk
