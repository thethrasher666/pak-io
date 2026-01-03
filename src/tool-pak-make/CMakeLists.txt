#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_executable(pak-make
    "main.cxx"
)

# Apply the default target settings.
set_target_defaults(pak-make)

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
