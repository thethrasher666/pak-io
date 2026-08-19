#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_library(pak-io.platform OBJECT
    "mem-sink.cxx"
    "mem-source.cxx"
)

# Set the target's defaults.
pk_set_target_defaults(pak-io.platform)

target_include_directories(pak-io.platform
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../../"
)

# Add alias library.
add_library(pak-io::platform ALIAS pak-io.platform)
