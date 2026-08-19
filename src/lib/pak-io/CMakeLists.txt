#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Configure files.
configure_file("version.hxx.in" "version.hxx")

# Define the target.
add_library(pak-io STATIC
    "pak-error.cxx"
    "pak-reader.cxx"
    "pak-writer.cxx"
)

# Set the target's defaults.
pk_set_target_defaults(pak-io)

# Set the target's include directories.
target_include_directories(pak-io
    PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# Set the target's dependencies.
target_link_libraries(pak-io
    PRIVATE
        pak-io::platform
)

# Add alias library.
add_library(pak-io::pak-io ALIAS pak-io)

# Add the platform-specific implementations.
add_subdirectory("posix")
add_subdirectory("win32")
