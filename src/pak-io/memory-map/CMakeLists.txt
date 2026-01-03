#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_library(memory-map
    $<$<PLATFORM_ID:Darwin,Linux>:memory-map-posix.cxx>
    $<$<PLATFORM_ID:Windows>:memory-map-win32.cxx>
)

# Create the alias target.
add_library(pak-io::memory-map ALIAS memory-map)

# Apply the default target settings.
set_target_defaults(memory-map)
