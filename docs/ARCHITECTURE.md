# Architecture

This document describes the design and implementation of the pak-io library and toolset.

## Overview

pak-io is structured as a core library (`pak-io`) with supporting utility libraries, three CLI tools, and a GUI viewer. The architecture emphasizes performance through parallel compression, memory-mapped I/O, and efficient file format design.

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  pak-make    │  │ pak-unmake   │  │  pak-info    │      │
│  │   (write)    │  │   (read)     │  │   (read)     │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │             pak-viewer (GUI - Python/Qt)             │   │
│  │           (subprocess wrapper around tools)          │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────┐
│                      Core Library                           │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                    pak_io                            │   │
│  │  - Archive creation and management                   │   │
│  │  - File addition with compression                    │   │
│  │  - File extraction with decompression                │   │
│  │  - Directory table management                        │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
          │                                   │
┌─────────────────────────┐    ┌─────────────────────────────┐
│  compression-queue      │    │      memory-map             │
│  - Thread pool          │    │  - Cross-platform I/O       │
│  - Parallel compression │    │  - POSIX (mmap)             │
│  - Job queue            │    │  - Windows (MapViewOfFile)  │
│  - LZ4 integration      │    │  - Flush management         │
└─────────────────────────┘    └─────────────────────────────┘
```

## Core Components

### pak_io Library

**Location:** `src/pak-io/pak-io.{hxx,cxx}`

The main library class that provides the public API for archive operations.

**Key Responsibilities:**
- Archive lifecycle management (open, close)
- File addition during write mode
- File extraction and listing during read mode
- Directory table construction and parsing
- Footer management

**Write Mode Operations:**
1. `open(path)` - Create new archive, initialize memory map
2. `add_file(identifier, source_path)` - Queue file for compression
3. `close()` - Finalize directory table and footer, write to disk

**Read Mode Operations:**
1. `open_for_reading(path)` - Open existing archive, read footer
2. `list_files()` - Return all file identifiers
3. `get_file_info()` - Return detailed metadata for all files
4. `extract_file(identifier, dest_path)` - Decompress and write file

**Design Decisions:**
- Single mode operation (read XOR write) for simplicity
- All files compressed in parallel before finalization
- Directory stored at end for single-pass writing
- Files stored contiguously for cache efficiency

### compression-queue

**Location:** `src/pak-io/compression-queue/compression-queue.{hxx,cxx}`

Thread pool-based parallel compression system.

**Architecture:**
```
┌──────────────────────────────────────────────────────┐
│                 compression_queue                    │
│                                                      │
│  ┌────────────┐     ┌──────────────────────────┐    │
│  │ Job Queue  │────▶│   Worker Threads (N)     │    │
│  │  (mutex)   │     │  - Pop job               │    │
│  └────────────┘     │  - LZ4 compress          │    │
│        ▲            │  - Store result          │    │
│        │            └──────────────────────────┘    │
│  ┌─────────────┐              │                     │
│  │ submit_job  │              ▼                     │
│  └─────────────┘    ┌──────────────────────────┐    │
│                     │   Completion Callback    │    │
│                     └──────────────────────────┘    │
└──────────────────────────────────────────────────────┘
```

**Threading Model:**
- Fixed-size thread pool (default: hardware concurrency)
- Lock-based job queue with condition variable
- Jobs processed FIFO
- Callback executed on worker thread after compression

**Synchronization:**
- `std::mutex` protects job queue
- `std::condition_variable` wakes workers
- `std::atomic<bool>` for shutdown flag
- Threads joined in destructor (RAII)

**LZ4 Integration:**
- Uses `LZ4_compress_default()` for compression
- Uses `LZ4_decompress_safe()` for decompression
- Allocates worst-case buffer: `LZ4_compressBound(size)`
- Returns actual compressed size

### memory-map

**Location:** `src/pak-io/memory-map/memory-map.{hxx,cxx}`

Cross-platform memory-mapped file I/O abstraction.

**Platform Implementations:**

**POSIX (`memory-map-posix.cxx`):**
```cpp
// Write mode
int fd = ::open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
::ftruncate(fd, initial_size);
void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0);

// Read mode
int fd = ::open(path, O_RDONLY);
void* addr = ::mmap(nullptr, size, PROT_READ,
                    MAP_SHARED, fd, 0);

// Sync
::msync(addr, size, MS_SYNC);

// Cleanup
::munmap(addr, size);
::close(fd);
```

**Windows (`memory-map-win32.cxx`):**
```cpp
// Write mode
HANDLE file = CreateFileW(..., GENERIC_READ | GENERIC_WRITE, ...);
HANDLE mapping = CreateFileMappingW(file, ..., PAGE_READWRITE, ...);
void* addr = MapViewOfFile(mapping, FILE_MAP_WRITE, ...);

// Read mode
HANDLE file = CreateFileW(..., GENERIC_READ, ...);
HANDLE mapping = CreateFileMappingW(file, ..., PAGE_READONLY, ...);
void* addr = MapViewOfFile(mapping, FILE_MAP_READ, ...);

// Sync
FlushViewOfFile(addr, size);

// Cleanup
UnmapViewOfFile(addr);
CloseHandle(mapping);
CloseHandle(file);
```

**Design Benefits:**
- Zero-copy I/O for large files
- OS-managed caching and prefetching
- Simplified pointer-based access
- Automatic dirty page tracking

## PAK File Format

### On-Disk Layout

```
┌────────────────────────────────────────────────────────┐
│                    File Data Section                   │
│  ┌──────────────────────────────────────────────────┐  │
│  │ File 0 (compressed LZ4 data)                     │  │
│  ├──────────────────────────────────────────────────┤  │
│  │ File 1 (compressed LZ4 data)                     │  │
│  ├──────────────────────────────────────────────────┤  │
│  │ ...                                              │  │
│  ├──────────────────────────────────────────────────┤  │
│  │ File N (compressed LZ4 data)                     │  │
│  └──────────────────────────────────────────────────┘  │
├────────────────────────────────────────────────────────┤
│                   Directory Section                    │
│  For each file:                                        │
│    - name_length: uint64_t                             │
│    - name: char[name_length]                           │
│    - compressed_size: uint64_t                         │
│    - uncompressed_size: uint64_t                       │
│    - offset: uint64_t (from start of file)             │
├────────────────────────────────────────────────────────┤
│                      Footer (24 bytes)                 │
│  - magic: uint32_t (0x50414B21 = "PAK!")               │
│  - version: uint32_t (currently 1)                     │
│  - entry_count: uint64_t                               │
│  - directory_offset: uint64_t                          │
└────────────────────────────────────────────────────────┘
```

### Format Rationale

**Directory at End:**
- Allows single-pass writing (don't need to know sizes upfront)
- Can append files without rebuilding entire archive
- Footer always at fixed offset from end (-24 bytes)

**Fixed-Width Integers:**
- `uint32_t` for magic and version (sufficient range)
- `uint64_t` for sizes and offsets (supports >4GB archives)
- Little-endian on all platforms for simplicity

**Per-File Compression:**
- Random access without decompressing entire archive
- Parallel compression during creation
- Failed compression of one file doesn't corrupt others
- Can skip decompressing unneeded files

**File Identifier vs Path:**
- Identifiers stored as strings (not necessarily filesystem paths)
- Allows virtual paths, namespacing
- Client controls naming convention

## CLI Tools

### pak-make

**Purpose:** Create PAK archives from TOML manifests

**Flow:**
```
TOML File
   ↓
Parse with toml++ → Expand globs → Resolve paths → pak_io::add_file()
   ↓                     ↓              ↓                ↓
manifest_entry     filesystem::     absolute      compression_queue
                   directory_       paths         (parallel)
                   iterator
```

**Key Features:**
- Wildcard expansion using filesystem iterator + regex
- Relative path resolution (relative to manifest file)
- Parallel compression via compression_queue
- Progress reporting

### pak-unmake

**Purpose:** Extract all files from PAK archive

**Flow:**
```
PAK File → pak_io::open_for_reading() → list_files()
                                            ↓
                            For each file: extract_file()
                                            ↓
                                    LZ4 decompress → Write to disk
                                            ↓
                            create_directories(parent_path)
```

**Key Features:**
- Automatic directory creation for nested files
- Progress reporting with success/failure counts
- Error handling per-file (continues on failure)

### pak-info

**Purpose:** Inspect PAK archive metadata

**Flow:**
```
PAK File → open_for_reading() → get_file_info()
                                      ↓
                          Calculate statistics (totals, ratios)
                                      ↓
                          Format table with std::format
```

**Key Features:**
- Compression ratio calculation
- Human-readable sizes (bytes/KB/MB)
- Aligned table output
- Overall archive statistics

### pak-viewer (GUI)

**Purpose:** Visual archive browser with preview

**Architecture:**
```
┌──────────────────────────────────────────────────┐
│               PySide6 Application                │
│                                                  │
│  ┌────────────────────┐  ┌────────────────────┐ │
│  │  File List Table   │  │  Preview Panel     │ │
│  │  - QTableWidget    │  │  - QTextEdit (text)│ │
│  │  - Name/Size/Ratio │  │  - QLabel (image)  │ │
│  └────────────────────┘  └────────────────────┘ │
│           │                        ▲             │
│           ▼                        │             │
│  ┌──────────────────────────────────────────┐   │
│  │       Extraction Thread (QThread)        │   │
│  │  subprocess.run([pak-unmake, ...])       │   │
│  └──────────────────────────────────────────┘   │
└──────────────────────────────────────────────────┘
                    │
                    ▼
    ┌────────────────────────────────┐
    │  Temporary Directory           │
    │  (cleaned up on exit)          │
    └────────────────────────────────┘
```

**Implementation Strategy:**
- Subprocess wrapper (simple, no bindings needed)
- `pak-info` for listing
- `pak-unmake` for extraction to temp dir
- Background thread for extraction (non-blocking UI)
- Format detection by file extension
- Preview types: images, text, hex dump

## Build System

### CMake Structure

```
CMakeLists.txt (root)
├── cmake/
│   ├── options.cmake       # Build options
│   ├── packages.cmake      # Find LZ4, Catch2
│   ├── targets.cmake       # Common target settings (C++23, warnings)
│   └── toolchain-defaults.cmake  # Compiler selection (Xcode.app)
└── src/
    ├── CMakeLists.txt
    ├── pak-io/
    │   ├── CMakeLists.txt          # pak-io library
    │   ├── compression-queue/
    │   │   └── CMakeLists.txt      # compression-queue library
    │   └── memory-map/
    │       └── CMakeLists.txt      # memory-map library
    ├── tool-pak-make/
    │   └── CMakeLists.txt          # pak-make executable
    ├── tool-pak-unmake/
    │   └── CMakeLists.txt          # pak-unmake executable
    └── tool-pak-info/
        └── CMakeLists.txt          # pak-info executable
```

### Presets

**CMakePresets.json** defines:
- `Debug`: No optimization, debug symbols
- `RelWithDebInfo`: Optimized with debug info
- `Release`: Full optimization, no debug info

All use Ninja generator and set build directory to `../builds/pak-io/{preset}/build`

### Compiler Requirements

**Minimum Versions:**
- **Clang 17+** (for `<format>` support)
- **GCC 13+** (for `<format>` support, GCC 14+ for `<print>`)
- **MSVC 19.37+** (for `<format>` support)

**C++ Features Used:**
- `std::format` (C++20) - formatted output
- `std::filesystem` - path manipulation, directory iteration
- Structured bindings
- `if constexpr`
- Template argument deduction

**Fallback Strategy:**
- Originally used `std::print` (C++23)
- Reverted to `std::cout`/`std::cerr` + `std::format` (C++20)
- Wider compiler compatibility

## Testing

### Test Structure

**Framework:** Catch2 v3.12.0

**Test Files:**
- `test-memory-map.cxx` - Memory mapping operations
- `test-compression-queue.cxx` - Thread pool and compression
- `test-pak-make.cxx` - Archive creation, manifest parsing
- `test-pak-unmake.cxx` - Archive extraction (library + CLI)
- `test-pak-info.cxx` - Archive inspection (CLI)

**Test Patterns:**

**Unit Tests (Library):**
```cpp
TEST_CASE("pak_io can extract files", "[pak-io]") {
    // Create test pak
    // Extract file
    // Verify contents match
}
```

**Integration Tests (CLI):**
```cpp
TEST_CASE("pak-unmake CLI extracts files correctly", "[cli]") {
    // Run executable via subprocess (popen)
    // Check exit code and output
    // Verify extracted files
}
```

**Temporary Files:**
- Atomic counter for unique temp directories
- RAII cleanup helpers
- Tests don't interfere with each other

## Performance Considerations

### Parallelism

**Compression Queue:**
- N worker threads (default: `std::thread::hardware_concurrency()`)
- Distributes CPU-bound LZ4 compression
- Near-linear speedup for multi-file archives

**Memory Mapping:**
- OS-level page cache
- Prefetching for sequential access
- Reduced syscall overhead

### Memory Usage

**Write Mode:**
- Each file compressed in-memory before writing
- Peak usage: largest uncompressed file + largest compressed file + directory table
- Memory map grows as files added

**Read Mode:**
- Only memory map (OS can page as needed)
- Decompression buffer: size of largest uncompressed file
- Can extract files larger than RAM (streaming decompression)

### File Size Limits

**Theoretical:**
- `uint64_t` offsets/sizes → 16 EB (exabytes) max file/archive size
- Practically limited by:
  - Available disk space
  - Memory mapping limits (OS-dependent)
  - Single file buffer allocation

## Error Handling

### Strategy

**Library:**
- Boolean return values for operations
- No exceptions from public API
- Internal exceptions caught and converted to `false`

**CLI Tools:**
- Exit code 0 on success, 1 on failure
- Error messages to `stderr`
- Continue processing on per-file failures when possible

**Memory Mapping:**
- Platform-specific error checking
- Invalid handles/pointers checked before use
- Resources cleaned up in destructors (RAII)

## Security Considerations

### Input Validation

**Archive Reading:**
- Magic number verification
- Bounds checking on offsets
- Compressed size limits (prevent decompression bombs)
- Path validation (prevent directory traversal)

**Manifest Parsing:**
- toml++ handles malformed TOML
- Filesystem access checked (file existence, readability)
- Glob expansion limited to manifest directory tree

### Known Limitations

- No encryption (compression only)
- No integrity verification (no checksums/signatures)
- No protection against malicious archives
- Assumes trusted input

## Future Enhancements

### Potential Features

1. **Incremental Updates**
   - Modify existing archives without full rebuild
   - Requires free space tracking, defragmentation

2. **Compression Options**
   - Multiple algorithms (zstd, zlib, etc.)
   - Per-file compression level control
   - Uncompressed storage for already-compressed formats

3. **Streaming API**
   - Extract without loading full file into memory
   - Write files incrementally

4. **Checksums**
   - CRC32 or SHA-256 per file
   - Archive-level integrity verification

5. **Metadata**
   - Timestamps, permissions, attributes
   - Custom per-file tags/properties

6. **Random Access Decompression**
   - LZ4 frame format with independent blocks
   - Seek within compressed files

### Design Constraints

**Backward Compatibility:**
- Version field in footer enables format evolution
- Readers can reject unsupported versions
- Directory entries can be extended with new fields

**ABI Stability:**
- Currently header-only or static library
- No guarantees for dynamic linking
- C API wrapper would enable stable ABI
