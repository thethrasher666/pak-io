#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_executable(pak-info
    "main.cxx"
)

# Apply the default target settings.
set_target_defaults(pak-info)

# Link the dependent libraries.
target_link_libraries(pak-info
    PRIVATE
        pak-io::pak-io
)

# Installation.
install(
    TARGETS
        pak-info
    RUNTIME DESTINATION "."
)
