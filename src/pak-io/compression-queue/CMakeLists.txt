#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Define the target.
add_library(compression-queue
    "compression-queue.cxx"
)

# Create the alias target.
add_library(pak-io::compression-queue ALIAS compression-queue)

# Apply the default target settings.
set_target_defaults(compression-queue)
