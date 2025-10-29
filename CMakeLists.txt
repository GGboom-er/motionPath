cmake_minimum_required(VERSION 3.12)
project(motionPath)

# Set default Maya version
if(NOT DEFINED MAYA_VERSION)
    set(MAYA_VERSION 2025)
endif()

# Find Maya
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Maya REQUIRED)

# Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Set compile flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_options(/wd4068 /EHsc)
    add_definitions(-D_HAS_ITERATOR_DEBUGGING=0 -D_SECURE_SCL=0 -D_SECURE_SCL_THROWS=0 -D_SECURE_SCL_WARN=0 -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -DTBB_USE_DEBUG=0 -D__TBB_NO_IMPLICIT_LINKAGE=1 -DNT_PLUGIN)
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
)

# Create target
add_library(motionPath SHARED ${SOURCES} ${HEADERS})

# Link libraries
target_include_directories(motionPath PUBLIC
    ${MAYA_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_directories(motionPath PUBLIC
    ${MAYA_LIBRARY_DIR}
)

target_link_libraries(motionPath PUBLIC
    ${MAYA_LIBRARIES}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(motionPath PROPERTIES
    COMPILE_DEFINITIONS "${MAYA_COMPILE_DEFINITIONS}"
    PREFIX ""
    SUFFIX ".mll")

if(WIN32)
    set_target_properties(motionPath PROPERTIES LINK_FLAGS "/export:initializePlugin /export:uninitializePlugin")
endif()

# Install
install(TARGETS motionPath
    DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/plug-ins/${MAYA_VERSION}"
)