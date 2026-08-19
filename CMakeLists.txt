#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Set the minimum CMake version.
cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

# Define the project.
project(PakIO
    VERSION         0.0.1.0
    DESCRIPTION     "A cross-platform file packer."
    HOMEPAGE_URL    "https://www.jamiekenyon.com/"
    LANGUAGES       CXX
)

# Include the dependent modules.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include("defaults")
include("options")
include("packages")

# Add the documentation.
if(PAK_BUILD_DOCS)
    add_subdirectory("docs")
endif()

# Add the sources.
add_subdirectory("src")

# Enable testing.
if(PAK_BUILD_TESTING)
    enable_testing()
    include("Catch")
    add_subdirectory("tests")
endif()
