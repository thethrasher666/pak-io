#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Use latest Xcode for C++23 support (without changing system default)
if(APPLE AND NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_C_COMPILER "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang" CACHE FILEPATH "C compiler")
    set(CMAKE_CXX_COMPILER "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++" CACHE FILEPATH "C++ compiler")
    set(CMAKE_OSX_SYSROOT "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk" CACHE PATH "macOS SDK path")
endif()

# Set the minimum MacOS version.
set(CMAKE_OSX_DEPLOYMENT_TARGET "15" CACHE STRING "Minimum OS X deployment version")

# Generate values from the current date that can be used in file configuration macros.
string(TIMESTAMP BUILD_YEAR  "%Y")
string(TIMESTAMP BUILD_MONTH "%m")
string(TIMESTAMP BUILD_DAY   "%d")
string(TIMESTAMP BUILD_TIME  "%H:%M:%S")
