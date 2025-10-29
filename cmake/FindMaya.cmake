#.rst:
# FindMaya
# --------
#
# Find Maya SDK libraries and includes
#
# This module defines:
#
# ::
#
#   MAYA_FOUND          - True if Maya SDK is found
#   MAYA_INCLUDE_DIRS   - Include directories for Maya headers
#   MAYA_LIBRARIES      - Libraries to link against
#   MAYA_LOCATION       - Installation directory of Maya
#   MAYA_VERSION        - Maya version (default: 2025)
#   MAYA_API_VERSION    - Maya API version number

# Set default Maya version if not specified
if(NOT DEFINED MAYA_VERSION)
    set(MAYA_VERSION 2025 CACHE STRING "Maya version")
endif()

# Platform-specific default paths
if(WIN32)
    # Windows default installation paths
    set(MAYA_DEFAULT_PATHS
        "C:/Program Files/Autodesk/Maya${MAYA_VERSION}"
        "C:/Program Files (x86)/Autodesk/Maya${MAYA_VERSION}"
        "$ENV{PROGRAMFILES}/Autodesk/Maya${MAYA_VERSION}"
        "$ENV{MAYA_LOCATION}"
    )
    set(MAYA_LIB_DIR "lib")
    set(MAYA_BIN_DIR "bin")
elseif(APPLE)
    # macOS default installation paths
    set(MAYA_DEFAULT_PATHS
        "/Applications/Autodesk/maya${MAYA_VERSION}/Maya.app/Contents"
        "$ENV{MAYA_LOCATION}"
    )
    set(MAYA_LIB_DIR "Maya.app/Contents/MacOS")
    set(MAYA_BIN_DIR "Maya.app/Contents/bin")
else()
    # Linux default installation paths
    set(MAYA_DEFAULT_PATHS
        "/usr/autodesk/maya${MAYA_VERSION}"
        "/usr/autodesk/maya${MAYA_VERSION}-x64"
        "$ENV{MAYA_LOCATION}"
    )
    set(MAYA_LIB_DIR "lib")
    set(MAYA_BIN_DIR "bin")
endif()

# Find Maya installation directory
find_path(MAYA_LOCATION
    NAMES
        include/maya/MFn.h
        devkit/include/maya/MFn.h
    PATHS
        ${MAYA_DEFAULT_PATHS}
    DOC "Maya installation directory"
)

if(NOT MAYA_LOCATION)
    if(Maya_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find Maya ${MAYA_VERSION}. Searched in: ${MAYA_DEFAULT_PATHS}")
    else()
        message(WARNING "Could not find Maya ${MAYA_VERSION}")
        return()
    endif()
endif()

# Find Maya include directory
find_path(MAYA_INCLUDE_DIR
    NAMES maya/MFn.h
    PATHS
        ${MAYA_LOCATION}/include
        ${MAYA_LOCATION}/devkit/include
    NO_DEFAULT_PATH
    DOC "Maya include directory"
)

# Set Maya API version based on Maya version
if(MAYA_VERSION EQUAL 2025)
    set(MAYA_API_VERSION 20250000)
elseif(MAYA_VERSION EQUAL 2024)
    set(MAYA_API_VERSION 20240000)
elseif(MAYA_VERSION EQUAL 2023)
    set(MAYA_API_VERSION 20230000)
elseif(MAYA_VERSION EQUAL 2022)
    set(MAYA_API_VERSION 20220000)
else()
    set(MAYA_API_VERSION ${MAYA_VERSION}00)
endif()

# Required Maya libraries
set(MAYA_LIBRARY_NAMES
    OpenMaya
    OpenMayaAnim
    OpenMayaUI
    OpenMayaRender
    Foundation
)

# Find each Maya library
set(MAYA_LIBRARIES)
foreach(MAYA_LIB ${MAYA_LIBRARY_NAMES})
    find_library(MAYA_${MAYA_LIB}_LIBRARY
        NAMES ${MAYA_LIB}
        PATHS
            ${MAYA_LOCATION}/${MAYA_LIB_DIR}
            ${MAYA_LOCATION}/lib
        NO_DEFAULT_PATH
        DOC "Maya ${MAYA_LIB} library"
    )

    if(MAYA_${MAYA_LIB}_LIBRARY)
        list(APPEND MAYA_LIBRARIES ${MAYA_${MAYA_LIB}_LIBRARY})
        mark_as_advanced(MAYA_${MAYA_LIB}_LIBRARY)
    else()
        if(Maya_FIND_REQUIRED)
            message(FATAL_ERROR "Could not find Maya library: ${MAYA_LIB}")
        endif()
    endif()
endforeach()

# Find devkit libraries path (for additional libraries)
find_path(MAYA_LIBRARY_DIR
    NAMES OpenMaya.lib OpenMaya.so libOpenMaya.dylib
    PATHS
        ${MAYA_LOCATION}/${MAYA_LIB_DIR}
        ${MAYA_LOCATION}/lib
    NO_DEFAULT_PATH
    DOC "Maya library directory"
)

# Set include directories
set(MAYA_INCLUDE_DIRS ${MAYA_INCLUDE_DIR})

# Add compile definitions for Maya API version
add_compile_definitions(MAYA_API_VERSION=${MAYA_API_VERSION})

# Handle the QUIETLY and REQUIRED arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Maya
    REQUIRED_VARS
        MAYA_LOCATION
        MAYA_INCLUDE_DIR
        MAYA_LIBRARIES
    VERSION_VAR
        MAYA_VERSION
    FAIL_MESSAGE
        "Could not find Maya ${MAYA_VERSION}. Please set MAYA_LOCATION to your Maya installation directory."
)

# Mark variables as advanced
mark_as_advanced(
    MAYA_LOCATION
    MAYA_INCLUDE_DIR
    MAYA_LIBRARY_DIR
)

# Create imported target for Maya
if(MAYA_FOUND AND NOT TARGET Maya::Maya)
    add_library(Maya::Maya INTERFACE IMPORTED)
    set_target_properties(Maya::Maya PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MAYA_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${MAYA_LIBRARIES}"
        INTERFACE_COMPILE_DEFINITIONS "MAYA_API_VERSION=${MAYA_API_VERSION}"
    )
endif()

# Print found information
if(MAYA_FOUND)
    message(STATUS "Found Maya ${MAYA_VERSION}")
    message(STATUS "  Location: ${MAYA_LOCATION}")
    message(STATUS "  Include: ${MAYA_INCLUDE_DIR}")
    message(STATUS "  Libraries: ${MAYA_LIBRARIES}")
    message(STATUS "  API Version: ${MAYA_API_VERSION}")
endif()
