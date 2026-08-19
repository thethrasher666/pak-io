#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_executable(test_pak_io
    "test-reader.cxx"
    "test-writer.cxx"
)

# Ensure tests can use the tools.
add_dependencies(test_pak_io pak-make pak-unmake)

# Set the defaults.
pk_set_target_defaults(test_pak_io)

# Set the target's dependencies.
target_link_libraries(test_pak_io
    PRIVATE
        pak-io::pak-io
        Catch2::Catch2WithMain
)

# Discover the tests.
catch_discover_tests(test_pak_io)
