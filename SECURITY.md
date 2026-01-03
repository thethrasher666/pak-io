# Security Policy

## Reporting Security Vulnerabilities

If you discover a security vulnerability in pak-io, please report it privately to avoid public disclosure before a fix is available.

**Contact:** jamie@example.com

Please include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

We will respond within 48 hours and work with you to understand and address the issue.

## Security Considerations

### Input Validation

pak-io performs basic validation when reading archives:
- Magic number verification (`0x50414B21`)
- Bounds checking on file offsets
- Directory table size validation

**However**, the library is designed for **trusted content only** and has the following limitations:

### Known Limitations

⚠️ **Do not process untrusted PAK archives from unknown sources.**

The library does NOT protect against:

1. **Decompression Bombs**: Maliciously crafted archives with extreme compression ratios could exhaust memory
2. **Path Traversal**: Extracted file paths are not sanitized for `..` sequences
3. **Symbolic Link Attacks**: No protection when extracting to directories with symlinks
4. **Resource Exhaustion**: Large archives could consume excessive disk space or memory
5. **Data Integrity**: No checksums or cryptographic signatures to verify contents
6. **Encryption**: Archives are compressed but not encrypted

### Recommendations

For production use with untrusted input:

1. **Validate Archive Sources**: Only process archives from trusted origins
2. **Sandbox Extraction**: Extract to isolated directories with limited permissions
3. **Resource Limits**: Set filesystem quotas and memory limits when processing archives
4. **Path Sanitization**: Validate extracted file paths before writing
5. **Size Limits**: Check compressed/uncompressed sizes before extraction
6. **Timeout Handling**: Implement timeouts for compression/decompression operations

### Example: Safe Extraction

```cpp
#include <pak-io/pak-io.hxx>
#include <filesystem>

bool safe_extract(const std::string& pak_path, const std::string& dest_dir) {
    // Only accept archives from trusted locations
    if (!is_trusted_source(pak_path)) {
        return false;
    }

    pak::pak_io reader;
    if (!reader.open_for_reading(pak_path)) {
        return false;
    }

    auto files = reader.list_files();

    // Check total uncompressed size
    auto file_infos = reader.get_file_info();
    std::uint64_t total_size = 0;
    for (const auto& info : file_infos) {
        total_size += info.uncompressed_size;

        // Reject excessive compression ratios
        if (info.uncompressed_size > info.compressed_size * 1000) {
            return false; // Possible decompression bomb
        }

        // Sanitize paths
        if (info.name.find("..") != std::string::npos) {
            return false; // Path traversal attempt
        }
    }

    // Reject archives exceeding size limit (e.g., 1GB)
    if (total_size > 1024 * 1024 * 1024) {
        return false;
    }

    // Extract to isolated directory
    std::filesystem::path safe_dest = std::filesystem::absolute(dest_dir);
    for (const auto& file_id : files) {
        std::filesystem::path dest_path = safe_dest / file_id;

        // Verify path is within destination directory
        auto canonical_dest = std::filesystem::weakly_canonical(dest_path);
        if (!canonical_dest.string().starts_with(safe_dest.string())) {
            return false; // Path escapes destination
        }

        if (!reader.extract_file(file_id, dest_path.string())) {
            return false;
        }
    }

    return true;
}
```

## Scope

This security policy applies to:
- The pak-io C++ library
- Command-line tools (pak-make, pak-unmake, pak-info)
- GUI viewer (pak-viewer.py)

## Updates

This policy may be updated as security issues are discovered or features are added. Check the repository for the latest version.

**Last updated:** January 11, 2026
