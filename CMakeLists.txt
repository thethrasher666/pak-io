#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Set the minimum CMake version.
cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

# Include pre-project definitions.
include("toolchain-defaults")

# Define the project.
project(linguist
    VERSION         1.0.0.0
    DESCRIPTION     "PAK-IO - A compound file format for hero assets."
    HOMEPAGE_URL    "https://www.pixel-slayer.com/"
    LANGUAGES       CXX C
)

set(CMAKE_INSTALL_MESSAGE LAZY)

# Include the dependent modules.
include("CTest")
include("options")
include("packages")
include("targets")

# Add the source directory.
add_subdirectory("src")

# Enable testing.
if(BUILD_TESTING)
    add_subdirectory("tests")
endif()
