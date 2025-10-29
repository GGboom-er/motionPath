//
//  PlatformFixes.h
//  MotionPath
//
//  Platform-specific fixes for Maya 2025 compatibility
//  This file must be included FIRST in all .cpp files
//

#ifndef PLATFORM_FIXES_H
#define PLATFORM_FIXES_H

// ===========================
// Windows Platform Fixes
// ===========================
#ifdef _WIN32
    // Prevent min/max macro pollution from Windows.h
    // These macros interfere with ternary operators and std::min/max
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    // Reduce Windows.h include overhead
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    // If Windows.h was already included, clean up the macros immediately
    #ifdef min
        #undef min
    #endif
    #ifdef max
        #undef max
    #endif
#endif

// ===========================
// Standard Library Includes
// ===========================
#include <algorithm>  // For std::min, std::max

// ===========================
// Utility Macros
// ===========================

// Convenient macro for Maya API findPlug calls with error checking
// Usage: TC_FIND_PLUG(node, "attrName", plug);
#define TC_FIND_PLUG(node, attrName, plug) \
    { \
        MStatus _status; \
        plug = node.findPlug(attrName, false, &_status); \
        if (_status != MS::kSuccess) { \
            MGlobal::displayWarning(MString("Failed to find plug: ") + attrName); \
        } \
    }

// Safe min/max alternatives (if needed, though std::min/max is preferred)
#define TC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TC_MAX(a, b) ((a) > (b) ? (a) : (b))

#endif // PLATFORM_FIXES_H
