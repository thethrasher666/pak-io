#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

include("FetchContent")

# Catch2.
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.15.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# Doxygen.
if(PAK_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
endif()

# TOML++.
FetchContent_Declare(TomlPlusPlus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(TomlPlusPlus)
