#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Set default target options.
function(set_target_defaults TRGT)
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
        PUBLIC
            $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${BUILD_WITH_SANITIZERS}>>:_DISABLE_STRING_ANNOTATION>
            $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${BUILD_WITH_SANITIZERS}>>:_DISABLE_VECTOR_ANNOTATION>

        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:_AMD64_>
            $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
            $<$<CXX_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN>
    )

    # Set the C++ standard version.
    target_compile_features(${TRGT} PRIVATE cxx_std_23)

    # Set C++ flags.
    target_compile_options(${TRGT}
        PRIVATE
            # Highest warning levels.
            $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>

            # Debugging information.
            $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/Zi>
            $<$<AND:$<CXX_COMPILER_ID:AppleClang,GNU>,$<CONFIG:Release>>:-g -g2>

            # Sanitizers (skip MinGW which doesn't support ASan).
            $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${BUILD_WITH_SANITIZERS}>>:/fsanitize=address>
            $<$<AND:$<CXX_COMPILER_ID:AppleClang,Clang,GNU>,$<BOOL:${BUILD_WITH_SANITIZERS}>,$<NOT:$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:GNU>>>>:-fsanitize=address>

            # Static analysis (MSVC only).
            $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<STREQUAL:${BUILD_STATIC_ANALYSIS_MODE},VisualStudio>>:/analyze:WX- /analyze:external->
    )

    # Set the target's include directories.
    target_include_directories(${TRGT}
        PRIVATE
            "${CMAKE_SOURCE_DIR}/src"
    )

    # Set linker options.
    target_link_options(${TRGT}
        PRIVATE
            $<$<AND:$<CXX_COMPILER_ID:AppleClang,Clang,GNU>,$<BOOL:${BUILD_WITH_SANITIZERS}>,$<NOT:$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:GNU>>>>:-fsanitize=address>
    )
endfunction(set_target_defaults)
