#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Include the dependent modules.
include(FetchContent)

# Compression library.
FetchContent_Declare(lz4
    GIT_REPOSITORY  https://github.com/lz4/lz4.git
    GIT_TAG         v1.10.0
)

# Download and make LZ4 available
FetchContent_GetProperties(lz4)
if(NOT lz4_POPULATED)
    FetchContent_Populate(lz4)

    # Create a library from LZ4 source files
    add_library(lz4 STATIC
        ${lz4_SOURCE_DIR}/lib/lz4.c
        ${lz4_SOURCE_DIR}/lib/lz4hc.c
        ${lz4_SOURCE_DIR}/lib/lz4frame.c
        ${lz4_SOURCE_DIR}/lib/xxhash.c
    )
    add_library(lz4::lz4 ALIAS lz4)

    # Tell CMake this is a C library
    set_target_properties(lz4 PROPERTIES LINKER_LANGUAGE C)

    target_include_directories(lz4 PUBLIC
        ${lz4_SOURCE_DIR}/lib
    )
endif()

# TOML library.
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY  https://github.com/marzer/tomlplusplus.git
    GIT_TAG         v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# Testing framework.
FetchContent_Declare(Catch2
  GIT_REPOSITORY    https://github.com/catchorg/Catch2.git
  GIT_TAG           v3.12.0
)
FetchContent_MakeAvailable(Catch2)
