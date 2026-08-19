#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Minimize the install messages.
set(CMAKE_INSTALL_MESSAGE LAZY)

# Generate values from the current date that can be used in file configuration macros.
string(TIMESTAMP BUILD_YEAR  "%Y")
string(TIMESTAMP BUILD_MONTH "%m")
string(TIMESTAMP BUILD_DAY   "%d")
string(TIMESTAMP BUILD_TIME  "%H:%M:%S")

# Set default options on all target types.
function(pk_set_target_defaults TRGT)
    # Set properties.
    set_target_properties(${TRGT}
        PROPERTIES
            COMPILE_WARNING_AS_ERROR  ON
            C_VISIBILITY_PRESET       default
            CXX_VISIBILITY_PRESET     default
            POSITION_INDEPENDENT_CODE ON
            VISIBILITY_INLINES_HIDDEN OFF
    )

    # Set the target's compile definitions.
    target_compile_definitions(${TRGT}
        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:_AMD64_>
            $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
            $<$<CXX_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN>
    )

    # Set the C++ standard version.
    # PUBLIC so that exported targets impose the same standard on consumers.
    target_compile_features(${TRGT} PUBLIC cxx_std_23)

    # Set C++ flags.
    target_compile_options(${TRGT}
        PRIVATE
            # Highest warning levels.
            $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>

            # Debugging information.
            $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/Zi>
            $<$<AND:$<CXX_COMPILER_ID:AppleClang,GNU>,$<CONFIG:Release>>:-g -g2>
    )
endfunction(pk_set_target_defaults)
