

#ifndef MOTIONPATHEDITCONTEXT_H
#define MOTIONPATHEDITCONTEXT_H

#include "MotionPathEditContextMenuWidget.h"
#include "MotionPathManager.h"
#include "MotionPath.h"

#include <maya/MFn.h>
#include <maya/MPxNode.h>
#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MGlobal.h>
#include <maya/MManipData.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>
#include <maya/M3dView.h>
#include <maya/MPoint.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>
#include <maya/MAnimControl.h>
#include <maya/MUIDrawManager.h>
#include <maya/MFrameContext.h>

#include <vector>
#include <time.h>

// Platform-specific includes for Caps Lock detection
#ifdef _WIN32
    #include <windows.h>
#elif defined(__APPLE__)
    #include <Carbon/Carbon.h>
    #include <CoreGraphics/CoreGraphics.h>
#endif

class MotionPathEditContextCmd: public MPxContextCommand
{
	public:
        MotionPathEditContextCmd();
		virtual MPxContext* makeObj();
		static void* creator();
};

class MotionPathEditContext: public MPxContext
{
	public:
		enum EditMode{
				kNoneMode = 0,
				kFrameEditMode = 1,
				kTangentEditMode = 2,
                kShiftKeyMode = 3};

		enum DrawMode{
				kDrawNone = 0,
				kDraw = 1,
				kStroke = 2};

		MotionPathEditContext();
		~MotionPathEditContext();

		// VP2-only: Legacy OpenGL versions removed
        virtual MStatus doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
        virtual MStatus doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
        virtual MStatus doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);

        virtual void toolOnSetup(MEvent& event);
        virtual void toolOffCleanup();


	private:
        int findPrefEditAxisFromVector(const MVector vec);

        void modifySelection(const MDoubleArray &selectedTimes, const bool ctrl, const bool shift);

        // VP2 versions that use MFrameContext (correct viewport handling)
        bool doPressVP2(MEvent &event, const MHWRender::MFrameContext &frameContext);
        void doDragVP2(MEvent &event, const MHWRender::MFrameContext &frameContext);
        void doReleaseVP2(MEvent &event, const MHWRender::MFrameContext &frameContext);

        // Legacy versions (deprecated, for compatibility only)
        bool doPressCommon(MEvent &event, const bool old);
        void doDragCommon(MEvent &event, const bool old);
        void doReleaseCommon(MEvent &event, const bool old);

		// Caps Lock detection and Draw mode
		bool isCapsLockOn() const;
		bool handleDrawModePress(MEvent &event, const bool old);
		bool handleDrawModeDrag(MEvent &event, const bool old);
		bool handleDrawModeRelease(MEvent &event, const bool old);

		// Draw mode preview rendering
		void initializeCircleVertices();
		void drawPreviewPath();
		void drawPreviewKeyframes();

		// Draw mode helper functions
		MVector getkeyScreenPosition(const double time);
		int getStrokeDirection(MVector directionalVector, const MDoubleArray &keys, const int selectedIndex);
		MVector getClosestPointOnPolyLine(const MVector &q);
		MVector getSpreadPointOnPolyLine(const int i, const int pointSize, const double strokeLenght, const std::vector<double> &segmentLenghts);
		double calculatePathLength(const MVectorArray &points);
		MVector samplePointOnPath(double t, const MVectorArray &points, double totalLength);

		MotionPath* selectedMotionPathPtr;
        EditMode currentMode;

        bool startedRecording;

        MGlobal::ListAdjustment listAdjustment;

        bool alongPreferredAxis;
        int prefEditAxis;

        bool shiftCachedDone;

        short initialX, initialY;
        short finalX, finalY;

        double lastSelectedTime;
        int selectedTangentId;
        MVector tangentWorldPosition;
        MVector keyWorldPosition;
        MVector lastWorldPosition;
        MPoint cameraPosition;

        M3dView activeView;  // Legacy only
        bool fsDrawn;

        MMatrix inverseCameraMatrix;

        // VP2: Store viewport state for the drag operation
        bool usingVP2;  // True if current operation is using VP2 path

        ContextMenuWidget*	ctxMenuWidget;

		// Draw mode state
		DrawMode drawMode;
		int drawSelectedKeyId;
		bool capsLockCached;
		bool capsLockValid;
		MVectorArray drawStrokePoints;
		MVector drawKeyWorldPosition;
		double drawSelectedTime;
		double drawMaxTime;
		double drawSteppedTime;
		clock_t drawInitialClock;

		// Static circle rendering optimization
		static std::vector<MPoint> circleVertices;
		static bool circleVerticesInitialized;
};

#endif
