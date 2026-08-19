//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <cstdint>

#define PAK_FILE_HEADER_MAGIC "PAKFILE"

/// The alignment, in bytes, of every section and payload within a pak file.
inline constexpr uint64_t PAK_ALIGNMENT = 32u;

/// The pak file header, located at offset zero.
struct alignas(32) PAK_FILE_HEADER
{
    char     magic[8] = { PAK_FILE_HEADER_MAGIC }; ///< The file indentifier.
    uint64_t name = 0;                             ///< The offset, from the start of the file, of the NUL-terminated pak file name.
    uint64_t count = 0;                            ///< Specifies the number of entries in the pak file.
    uint32_t version = 0;                          ///< Specifies the version of the library that was used to create the pak file.
    uint32_t reserved = 0;                         ///< Reserved, must be zero.
};

// Ensure the size of the struct is known and that there is no invisible padding.
static_assert(sizeof(PAK_FILE_HEADER) == 32);

/// The header of each entry in an pak file. The entry table immediately follows the file header.
struct alignas(32) PAK_ENTRY
{
    uint64_t identifier = 0; ///< The offset, from the start of the file, of the NUL-terminated identifier (filename stem).
    uint64_t size = 0;       ///< Specifies the uncompressed size of the entry.
    uint64_t data = 0;       ///< The offset, from the start of the file, of the entry's payload.
    uint64_t filename = 0;   ///< The offset, from the start of the file, of the NUL-terminated original filename (as supplied in the manifest, including its extension).
};

// Ensure the size of the struct is known and that there is no invisible padding.
static_assert(sizeof(PAK_ENTRY) == 32);
