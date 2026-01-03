# pak-io

A high-performance C++23 library and toolset for creating, extracting, and inspecting PAK archives with LZ4 compression.

## Features

- **Fast Compression**: LZ4-based compression with parallel processing via worker thread pool
- **Cross-Platform**: POSIX and Windows support with memory-mapped file I/O
- **Complete Toolset**: CLI tools for creating, extracting, and inspecting archives
- **GUI Viewer**: PySide6-based graphical viewer with file preview capabilities
- **Wildcard Support**: Glob patterns for flexible file selection

## Tools

### pak-make
Create PAK archives from TOML manifests:

```bash
pak-make manifest.toml output.pak
```

**Example manifest:**
```toml
version = "1.0"
compression = "lz4"

files = [
    "data/*.png",
    "config.json",
    "levels/*.dat"
]
```

### pak-unmake
Extract all files from a PAK archive:

```bash
pak-unmake archive.pak output_directory/
```

### pak-info
Inspect PAK archive contents:

```bash
pak-info archive.pak
```

**Output:**
```
PAK Archive Information
=======================

File: archive.pak
Files: 127
Total Uncompressed: 45.2 MB
Total Compressed: 12.8 MB
Overall Compression: 71.7%

Contents:
=========

Filename                   Compressed     Uncompressed     Ratio      Offset
--------------------  ---------------  ---------------  --------  ----------
textures/hero.png              45231            89234     49.3%           0
levels/level1.dat             128456           456789     71.9%       45231
...
```

### pak-viewer (GUI)
Visual inspection tool with preview capabilities:

```bash
cd tools
pip install -r requirements.txt
python pak-viewer.py [archive.pak]
```

Features:
- Browse archive contents
- View compression statistics
- Preview images (PNG, JPG, BMP, GIF)
- Preview text files (JSON, XML, TOML, Markdown, etc.)
- Hex dump for binary files

## Building

**Requirements:**
- C++23-capable compiler (Clang 17+, GCC 14+, MSVC 19.37+)
- CMake 3.25+

**Build steps:**
```bash
cmake --preset Debug
cmake --build --preset Debug
ctest --preset Debug
```

**macOS with specific Xcode:**
The project is configured to use `/Applications/Xcode.app` if available, allowing you to use a newer Xcode without changing your system default.

## Library Usage

```cpp
#include <pak-io/pak-io.hxx>

// Create archive
pak::pak_io writer;
writer.open("output.pak");
writer.add_file("config.json", "/path/to/config.json");
writer.add_file("data.bin", "/path/to/data.bin");
writer.close();

// Read archive
pak::pak_io reader;
reader.open_for_reading("output.pak");
auto files = reader.list_files();
reader.extract_file("config.json", "/output/path/config.json");
```

## File Format

PAK archives use a simple, efficient format:
```
[Compressed File Data][Directory Table][Footer]
```

**Footer structure (24 bytes):**
- Magic number (4 bytes): `0x50414B21` ("PAK!")
- Version (4 bytes): Format version
- Entry count (8 bytes): Number of files
- Directory offset (8 bytes): Offset to directory table

**Directory entries:**
- Name length + name string
- Compressed size (8 bytes)
- Uncompressed size (8 bytes)
- Offset (8 bytes)

Files are compressed individually using LZ4, allowing random access without decompressing the entire archive.

## Documentation

For detailed architecture and design decisions, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.

See [LICENSE](LICENSE) for details.
