//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/pak-writer.hxx"
#include "pak-io/pak-error.hxx"
#include "pak-io/platform/mem-sink.hxx"
#include "pak-io/pod.hxx"
#include "pak-io/version.hxx"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string>
#include <string_view>

namespace pk
{
    namespace
    {
        constexpr std::size_t copyChunkSize = 64u * 1024u;

        [[nodiscard]] constexpr auto alignUp(std::uint64_t const value) -> std::uint64_t
        {
            return (value + PAK_ALIGNMENT - 1u) & ~(PAK_ALIGNMENT - 1u);
        }

        /// An entry whose position within the pak file has already been decided.
        struct PlannedEntry
        {
            std::filesystem::path path;
            std::uint64_t         identifier{}; ///< Offset of the identifier, from the start of the file.
            std::uint64_t         filename{};   ///< Offset of the original filename, from the start of the file.
            std::uint64_t         size{};       ///< Size of the payload, in bytes.
            std::uint64_t         data{};       ///< Offset of the payload, from the start of the file.
        };

        /// The layout of the pak file, computed before anything is written.
        struct Layout
        {
            std::string               strings;             ///< The string table; NUL-terminated names, in write order.
            std::vector<PlannedEntry> entries;             ///< The entries, in write order.
            std::uint64_t             name{};              ///< Offset of the pak file name, from the start of the file.
            std::uint64_t             stringTableOffset{}; ///< Offset of the string table, from the start of the file.
            std::uint64_t             dataOffset{};        ///< Offset of the first payload, from the start of the file.
        };

        /// Works out where every section, name and payload lives. The sink is write-only, so nothing can be patched up afterwards.
        /// \param path The path of the pak file being created.
        /// \param files The files to include in the pak file.
        /// \return The layout, or an error if a source file could not be inspected.
        [[nodiscard]] auto planLayout(std::filesystem::path const& path, std::vector<std::filesystem::path> const& files) -> std::expected<Layout, std::error_code>
        {
            Layout layout;
            layout.entries.reserve(files.size());
            layout.stringTableOffset = sizeof(PAK_FILE_HEADER) + static_cast<std::uint64_t>(files.size()) * sizeof(PAK_ENTRY);

            auto const appendString = [&layout](std::string_view const value)
            {
                auto const offset = layout.stringTableOffset + layout.strings.size();
                layout.strings.append(value);
                layout.strings.push_back('\0');
                return offset;
            };

            layout.name = appendString(path.stem().string());

            for (auto const& file : files)
            {
                std::error_code error;
                auto const      size = std::filesystem::file_size(file, error);
                if (error)
                {
                    return std::unexpected(error);
                }

                layout.entries.push_back(
                PlannedEntry{ .path = file, .identifier = appendString(file.stem().string()), .filename = appendString(file.filename().string()), .size = size });
            }

            layout.dataOffset = alignUp(layout.stringTableOffset + layout.strings.size());

            auto offset = layout.dataOffset;
            for (auto& entry : layout.entries)
            {
                entry.data = offset;
                offset = alignUp(offset + entry.size);
            }
            return layout;
        }

        void pad(MemSinkPlatform& sink, std::uint64_t count)
        {
            constexpr std::array<std::byte, PAK_ALIGNMENT> zeroes{};

            while (count != 0)
            {
                auto const chunk = std::min<std::uint64_t>(count, zeroes.size());
                sink.write(zeroes.data(), static_cast<std::size_t>(chunk));
                count -= chunk;
            }
        }

        [[nodiscard]] auto copyPayload(MemSinkPlatform& sink, PlannedEntry const& entry) -> std::expected<void, std::error_code>
        {
            std::ifstream file(entry.path, std::ios::binary);
            if (!file.good())
            {
                return std::unexpected(makeErrorCode(ErrorCode::FailedToOpenFileForReading));
            }

            std::vector<char> buffer(copyChunkSize);
            for (std::uint64_t remaining = entry.size; remaining != 0;)
            {
                auto const chunk = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, buffer.size()));
                if (!file.read(buffer.data(), chunk))
                {
                    return std::unexpected(makeErrorCode(ErrorCode::FailedToReadSourceFile));
                }

                sink.write(buffer.data(), static_cast<std::size_t>(chunk));
                remaining -= static_cast<std::uint64_t>(chunk);
            }

            return {};
        }

        [[nodiscard]] auto writeLayout(MemSinkPlatform& sink, Layout const& layout) -> std::expected<void, std::error_code>
        {
            try
            {
                PAK_FILE_HEADER header;
                header.name = layout.name;
                header.count = layout.entries.size();
                header.version = version::ordinal();
                sink.write(&header, sizeof(header));

                for (auto const& planned : layout.entries)
                {
                    PAK_ENTRY entry;
                    entry.identifier = planned.identifier;
                    entry.size = planned.size;
                    entry.data = planned.data;
                    entry.filename = planned.filename;
                    sink.write(&entry, sizeof(entry));
                }

                sink.write(layout.strings.data(), layout.strings.size());

                auto offset = layout.stringTableOffset + layout.strings.size();
                for (auto const& planned : layout.entries)
                {
                    pad(sink, planned.data - offset);

                    if (auto const result = copyPayload(sink, planned); !result)
                    {
                        return result;
                    }

                    offset = planned.data + planned.size;
                }
            }
            catch (std::system_error const& error)
            {
                return std::unexpected(error.code());
            }
            catch (std::exception const&)
            {
                return std::unexpected(makeErrorCode(ErrorCode::FailedToWriteFile));
            }

            return {};
        }
    } // namespace

    PakWriter::PakWriter()
    {
    }

    PakWriter::~PakWriter()
    {
    }

    void PakWriter::close()
    {
        if (_memSink)
        {
            _memSink->close();
            _memSink.reset();
        }
    }

    auto PakWriter::process(std::filesystem::path const& path, std::vector<std::filesystem::path> const& files) -> std::expected<void, std::error_code>
    {
        if (_memSink)
        {
            return std::unexpected(makeErrorCode(ErrorCode::FileAlreadyOpenForWriting));
        }

        auto const layout = planLayout(path, files);
        if (!layout)
        {
            return std::unexpected(layout.error());
        }

        _memSink = std::make_unique<MemSinkPlatform>();
        if (auto const result = _memSink->open(path); !result)
        {
            _memSink.reset();
            return result;
        }

        auto const result = writeLayout(*_memSink, *layout);
        close();

        if (!result)
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            return result;
        }

        return {};
    }

    void PakWriter::write(void const* data, std::size_t size)
    {
        _memSink->write(data, size);
    }
} // namespace pk
