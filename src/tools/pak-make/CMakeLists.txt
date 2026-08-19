#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_executable(pak-make
    "error.cxx"
    "main.cxx"
    "manifest.cxx"
)

# Set the preprocessor definitions.
target_compile_definitions(pak-make
    PRIVATE
        TOML_EXCEPTIONS=0
)

# Link the dependent libraries.
target_link_libraries(pak-make
    PRIVATE
        pak-io::pak-io
        tomlplusplus::tomlplusplus
)

# Installation.
install(
    TARGETS
        pak-make
    RUNTIME DESTINATION "."
)
