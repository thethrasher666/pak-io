#
# Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
#

# Find Python and PySide6 tools
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Find pyside6-uic tool
find_program(PYSIDE6_UIC pyside6-uic REQUIRED)

# Generate Python UI file from Qt Designer .ui file
set(UI_FILE "${CMAKE_CURRENT_SOURCE_DIR}/pak-viewer.ui")
set(UI_PY_FILE "${CMAKE_CURRENT_BINARY_DIR}/pak_viewer_ui.py")

# Command to generate the Python UI file
add_custom_command(
    OUTPUT
        ${UI_PY_FILE}
    COMMAND
        ${PYSIDE6_UIC} ${UI_FILE} -o ${UI_PY_FILE}
    DEPENDS
        ${UI_FILE}
    COMMENT
        "Generating Python UI from pak-viewer.ui"
)

# Custom target to build the UI file
add_custom_target(pak-viewer-ui ALL
    DEPENDS
        ${UI_PY_FILE}
)

install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/__init__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/__main__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/pak-viewer.py"
        ${UI_PY_FILE}
    DESTINATION
        "pak-viewer"
)
