#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Configure files.
configure_file("version.hxx.in" "pak-io/version.hxx" @ONLY)

# Add the submodules.
add_subdirectory("compression-queue")
add_subdirectory("memory-map")

# Define the target.
add_library(pak-io
    "pak-io.cxx"
)

# Create the alias target.
add_library(pak-io::pak-io ALIAS pak-io)

# Apply the default target settings.
set_target_defaults(pak-io)

# Specify the public include directories.
target_include_directories(pak-io PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Link the dependent libraries.
target_link_libraries(pak-io
    PRIVATE
        lz4::lz4
        pak-io::compression-queue
        pak-io::memory-map
)
