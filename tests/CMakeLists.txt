#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_executable(pak-tests
    "test-compression-queue.cxx"
    "test-memory-map.cxx"
    "test-pak-info.cxx"
    "test-pak-make.cxx"
    "test-pak-unmake.cxx"
)

# Apply the default target settings.
set_target_defaults(pak-tests)

# Link the dependent libraries.
target_link_libraries(pak-tests
    PRIVATE
        Catch2::Catch2WithMain
        pak-io::pak-io
)

# Add compile definition for pak-make executable path
target_compile_definitions(pak-tests
    PRIVATE
        PAK_MAKE_EXECUTABLE="$<TARGET_FILE:pak-make>"
        PAK_UNMAKE_EXECUTABLE="$<TARGET_FILE:pak-unmake>"
        PAK_INFO_EXECUTABLE="$<TARGET_FILE:pak-info>"
        TEST_MANIFEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/sample.toml"
)

# Discover the tests.
include("Catch")
catch_discover_tests(pak-tests)
