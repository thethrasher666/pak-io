#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_executable(pak-unmake
    "main.cxx"
)

# Apply the default target settings.
set_target_defaults(pak-unmake)

# Link the dependent libraries.
target_link_libraries(pak-unmake
    PRIVATE
        pak-io::pak-io
)

# Installation.
install(
    TARGETS
        pak-unmake
    RUNTIME DESTINATION "."
)
