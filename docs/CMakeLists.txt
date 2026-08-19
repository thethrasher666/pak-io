#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Set Doxygen defaults.
set(DOXYGEN_EXTRACT_ALL          YES)
set(DOXYGEN_GENERATE_LATEX       NO)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_SOURCE_DIR}/docs/index.md")
set(DOXYGEN_QUIET                YES)
set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
set(DOXYGEN_WARN_IF_DOC_ERROR    YES)
set(DOXYGEN_WARN_AS_ERROR        YES)

# State which files require Doxygen processing.
file(GLOB_RECURSE DOC_MARKDOWN   "${CMAKE_SOURCE_DIR}/docs/*.md")
file(GLOB_RECURSE SRC_HEADERS    "${CMAKE_SOURCE_DIR}/src/*.hxx")
set(DOXYGEN_SOURCES              ${DOC_MARKDOWN} ${SRC_HEADERS})

# Define the Doxygen target.
doxygen_add_docs(doxygen ${DOXYGEN_SOURCES} ALL USE_STAMP_FILE COMMENT "Generating documentation.")
