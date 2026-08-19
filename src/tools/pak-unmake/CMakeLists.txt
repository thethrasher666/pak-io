#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_executable(pak-unmake
    "main.cxx"
)

# Link the dependent libraries.
target_link_libraries(pak-unmake
    PRIVATE
        pak-io::pak-io
        tomlplusplus::tomlplusplus
)

# Installation.
install(
    TARGETS
        pak-unmake
    RUNTIME DESTINATION "."
)
