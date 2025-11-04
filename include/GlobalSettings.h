
#ifndef GLOBALSETTINGS_H
#define GLOBALSETTINGS_H

#include <maya/MColor.h>
#include <maya/MMatrix.h>

#include <map>

class GlobalSettings
{
	public:
        // Size multipliers for different frame types - eliminates magic numbers
        static constexpr double KEYFRAME_SIZE_MULTIPLIER = 1.5;      // Keyframe marker size
        static constexpr double CURRENT_FRAME_SIZE_MULTIPLIER = 2.2; // Current frame marker size
        static constexpr double SELECTED_KEY_SIZE_MULTIPLIER = 1.2;  // Selected keyframe size
        static constexpr double BLACK_BACKGROUND_FACTOR = 1.2;       // Background outline factor

        enum DrawMode{
            kWorldSpace = 0,
            kCameraSpace = 1};

		static double startTime;
		static double endTime;
		static double framesBack;
		static double framesFront;
		static MColor pathColor;
		static MColor currentFrameColor;
		static MColor tangentColor;
		static MColor brokenTangentColor;
        static MColor bufferPathColor;
        static MColor weightedPathTangentColor;
        static MColor weightedPathColor;
        static MColor frameLabelColor;
		static MColor keyframeLabelColor;
		static double pathSize;
		static double frameSize;
		static double keyframeLabelSize;      // Size of keyframe number labels
		static double frameLabelSize;         // Size of regular frame number labels
		static bool showTangents;
        static bool showKeyFrames;
        static bool showKeyFrameNumbers;
        static bool showFrameNumbers;
        static bool showRotationKeyFrames;
        static bool showPath;
        static double drawTimeInterval;
        static int drawFrameInterval;
		static MMatrix cameraMatrix;
        static int portWidth;
        static int portHeight;
        static bool lockedMode;
        static bool enabled;
        static bool alternatingFrames;
        static bool lockedModeInteractive;
        static bool usePivots;
        static int strokeMode;
        static DrawMode motionPathDrawMode;

        // Draw preview settings
        static int drawKeyframeCount;          // Number of keyframes to create when drawing
        static MColor previewPathColor;        // Preview path color (distinct from real path)
        static MColor previewKeyframeColor;    // Preview keyframe marker color
};

#endif
