cmake_minimum_required(VERSION 3.12)
project(motionPath)

# Set default Maya version
if(NOT DEFINED MAYA_VERSION)
    set(MAYA_VERSION 2025)
endif()

# Find Maya
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Maya REQUIRED)

# Add Maya's Qt to CMAKE_PREFIX_PATH if not already set
# IMPORTANT: Maya 2025 uses Qt 5.15, NOT Qt6!
if(NOT CMAKE_PREFIX_PATH)
    if(WIN32)
        list(APPEND CMAKE_PREFIX_PATH "${MAYA_LOCATION}/bin")
    elseif(APPLE)
        list(APPEND CMAKE_PREFIX_PATH "${MAYA_LOCATION}/Maya.app/Contents/MacOS")
    else()
        list(APPEND CMAKE_PREFIX_PATH "${MAYA_LOCATION}/bin")
    endif()
    message(STATUS "Set CMAKE_PREFIX_PATH to Maya's Qt: ${CMAKE_PREFIX_PATH}")
endif()

# Qt5 Configuration (Maya 2025 uses Qt 5.15.x)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Try to find Qt5 first (Maya's version)
find_package(Qt5 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt5_FOUND)
    message(WARNING "Qt5 not found in CMAKE_PREFIX_PATH")
    message(STATUS "Trying Maya's Qt location: ${MAYA_LOCATION}/bin")
    set(CMAKE_PREFIX_PATH "${MAYA_LOCATION}/bin" ${CMAKE_PREFIX_PATH})
    find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
endif()

message(STATUS "Using Qt version: ${Qt5_VERSION}")

# Set compile flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-specific compiler options
if(WIN32)
    add_compile_options(/wd4068 /EHsc)
    add_definitions(
        -D_HAS_ITERATOR_DEBUGGING=0
        -D_SECURE_SCL=0
        -D_SECURE_SCL_THROWS=0
        -D_SECURE_SCL_WARN=0
        -D_CRT_SECURE_NO_DEPRECATE
        -D_CRT_NONSTDC_NO_DEPRECATE
        -DTBB_USE_DEBUG=0
        -D__TBB_NO_IMPLICIT_LINKAGE=1
        -DNT_PLUGIN
        -DNOMINMAX          # Critical: Prevent Windows.h min/max macros
        -D_BOOL             # Maya boolean type
        -DREQUIRE_IOSTREAM  # Maya requirement
    )
endif()

# Source files
set(SOURCES
    source/animCurveUtils.cpp
    source/BufferPath.cpp
    source/CameraCache.cpp
    source/ContextUtils.cpp
    source/DrawUtils.cpp
    source/GlobalSettings.cpp
    source/KeyClipboard.cpp
    source/Keyframe.cpp
    source/MotionPath.cpp
    source/MotionPathCmd.cpp
    source/MotionPathDrawContext.cpp
    source/MotionPathEditContext.cpp
    source/MotionPathEditContextMenuWidget.cpp
    source/MotionPathManager.cpp
    source/MotionPathOverride.cpp
    source/PluginMain.cpp
    source/Vp2DrawUtils.cpp
)

# Headers
set(HEADERS
    include/animCurveUtils.h
    include/BufferPath.h
    include/CameraCache.h
    include/ContextUtils.h
    include/DrawUtils.h
    include/GlobalSettings.h
    include/KeyClipboard.h
    include/Keyframe.h
    include/MotionPath.h
    include/MotionPathCmd.h
    include/MotionPathDrawContext.h
    include/MotionPathEditContext.h
    include/MotionPathEditContextMenuWidget.h
    include/MotionPathManager.h
    include/MotionPathOverride.h
    include/Vp2DrawUtils.h
    include/PlatformFixes.h
)

# Create target
add_library(motionPath SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(motionPath PUBLIC
    ${MAYA_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Library directories
target_link_directories(motionPath PUBLIC
    ${MAYA_LIBRARY_DIR}
)

# Link libraries
target_link_libraries(motionPath PUBLIC
    ${MAYA_LIBRARIES}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

# Set target properties
set_target_properties(motionPath PROPERTIES
    COMPILE_DEFINITIONS "${MAYA_COMPILE_DEFINITIONS}"
    PREFIX ""
    SUFFIX ".mll"
)

# Windows-specific linker settings
if(WIN32)
    set_target_properties(motionPath PROPERTIES
        LINK_FLAGS "/export:initializePlugin /export:uninitializePlugin"
    )
endif()

# Install
install(TARGETS motionPath
    DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/plug-ins/${MAYA_VERSION}"
)

# Print configuration summary
message(STATUS "==================================================")
message(STATUS "tcMotionPath Plugin Configuration")
message(STATUS "==================================================")
message(STATUS "Maya Version: ${MAYA_VERSION}")
message(STATUS "Maya Location: ${MAYA_LOCATION}")
message(STATUS "Qt5 Version: ${Qt5_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Output: plug-ins/${MAYA_VERSION}/motionPath.mll")
message(STATUS "==================================================")
