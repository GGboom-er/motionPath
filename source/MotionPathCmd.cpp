#include <maya/MPointArray.h>
#include <maya/MFnNurbsCurve.h>
#include "MotionPathCmd.h"

extern MotionPathManager mpManager;


void* MotionPathCmd::creator()
{
	return new MotionPathCmd;
}

MotionPathCmd::MotionPathCmd()
{
	this->animCurveChangePtr = NULL;
    this->dgModifierPtr = NULL;
	this->animUndoable = false;
    this->dgUndoable = false;
    this->keySelectionUndoable = false;
    this->selectionUndoable = false;
}

MotionPathCmd::~MotionPathCmd()
{
	if (this->animCurveChangePtr)
		delete this->animCurveChangePtr;
    
    if (this->dgModifierPtr)
  		delete this->dgModifierPtr;
}

bool MotionPathCmd::isUndoable() const
{
	return this->animUndoable || this->dgUndoable || this->keySelectionUndoable|| this->selectionUndoable;
}

MColor getColorFromArg(const MArgDatabase &argData, const char *flagName)
{
    double r, g, b;
    argData.getFlagArgument(flagName, 0, r);
    argData.getFlagArgument(flagName, 1, g);
    argData.getFlagArgument(flagName, 2, b);
    return MColor(r, g, b);
}


/*
 * tcMotionPathCmd - Command Flags Documentation
 *
 * This command provides control over the Motion Path visualization and editing system.
 *
 * =============================================================================
 * GENERAL CONTROL FLAGS
 * =============================================================================
 *
 * -e / -enable <boolean>
 *     Enable or disable the Motion Path plugin functionality.
 *     When enabled, initializes viewports and callbacks.
 *     When disabled, cleans up all Motion Path resources.
 *     Example: cmds.tcMotionPathCmd(enable=True)
 *
 * -gsl / -getCurrentSL
 *     Query: Returns the current selection list managed by Motion Path.
 *     Example: cmds.tcMotionPathCmd(getCurrentSL=True)
 *
 * -rdt / -refreshdt
 *     Refresh the display time range. Forces update of cached keyframe data.
 *     Example: cmds.tcMotionPathCmd(refreshdt=True)
 *
 * =============================================================================
 * TIME RANGE FLAGS
 * =============================================================================
 *
 * -bf / -framesBefore <int>
 *     Set number of frames to display before the current time.
 *     Minimum: 0, Default: 20
 *     Example: cmds.tcMotionPathCmd(framesBefore=30)
 *
 * -af / -framesAfter <int>
 *     Set number of frames to display after the current time.
 *     Minimum: 0, Default: 20
 *     Example: cmds.tcMotionPathCmd(framesAfter=30)
 *
 * -tfr / -frameRange <int> <int>
 *     Set absolute frame range [start, end] for display.
 *     Example: cmds.tcMotionPathCmd(frameRange=[1, 100])
 *
 * =============================================================================
 * DISPLAY VISIBILITY FLAGS
 * =============================================================================
 *
 * -sp / -showPath <boolean>
 *     Show or hide the motion path curve.
 *     Default: True
 *     Example: cmds.tcMotionPathCmd(showPath=True)
 *
 * -st / -showTangents <boolean>
 *     Show or hide tangent handles on keyframes.
 *     Default: False
 *     Example: cmds.tcMotionPathCmd(showTangents=True)
 *
 * -sk / -showKeyFrames <boolean>
 *     Show or hide keyframe markers on the path.
 *     Default: True
 *     Example: cmds.tcMotionPathCmd(showKeyFrames=True)
 *
 * -srk / -showRotationKeyFrames <boolean>
 *     Show or hide rotation keyframe markers.
 *     Default: True
 *     Example: cmds.tcMotionPathCmd(showRotationKeyFrames=True)
 *
 * -skn / -showKeyFrameNumbers <boolean>
 *     Show or hide keyframe index numbers on keyframes.
 *     Default: False
 *     Example: cmds.tcMotionPathCmd(showKeyFrameNumbers=True)
 *
 * -sfn / -showFrameNumbers <boolean>
 *     Show or hide frame numbers along the path.
 *     Default: False
 *     Example: cmds.tcMotionPathCmd(showFrameNumbers=True)
 *
 * =============================================================================
 * DISPLAY STYLE FLAGS
 * =============================================================================
 *
 * -alf / -alternatingFrames <boolean>
 *     Enable alternating colors for frame markers.
 *     Helps distinguish adjacent frames visually.
 *     Default: False
 *     Example: cmds.tcMotionPathCmd(alternatingFrames=True)
 *
 * -up / -usePivots <boolean>
 *     Use object pivot points instead of object centers for path display.
 *     Useful for showing rotation pivot trajectories.
 *     Default: False
 *     Example: cmds.tcMotionPathCmd(usePivots=True)
 *
 * =============================================================================
 * SIZE FLAGS
 * =============================================================================
 *
 * -ps / -pathSize <double>
 *     Set the line width of the motion path curve.
 *     Range: 0.1 - 100.0, Default: 3.0
 *     Example: cmds.tcMotionPathCmd(pathSize=5.0)
 *
 * -fs / -frameSize <double>
 *     Set the size of keyframe marker points.
 *     Range: 0.1 - 100.0, Default: 7.0
 *     Example: cmds.tcMotionPathCmd(frameSize=10.0)
 *
 * =============================================================================
 * COLOR FLAGS (RGB values in 0.0-1.0 range)
 * =============================================================================
 *
 * -pc / -pathColor <r> <g> <b>
 *     Set the color of the motion path curve.
 *     Default: (0, 0.698, 0.933) - Cyan
 *     Example: cmds.tcMotionPathCmd(pathColor=[1.0, 0.0, 0.0])
 *
 * -cfc / -currentFrameColor <r> <g> <b>
 *     Set the color of the current frame marker.
 *     Default: (1.0, 1.0, 0.0) - Yellow
 *     Example: cmds.tcMotionPathCmd(currentFrameColor=[1.0, 0.5, 0.0])
 *
 * -tc / -tangentColor <r> <g> <b>
 *     Set the color of tangent handles.
 *     Default: (0.545, 0.412, 0.412) - Brown
 *     Example: cmds.tcMotionPathCmd(tangentColor=[0.5, 0.5, 0.5])
 *
 * -btc / -brokenTangentColor <r> <g> <b>
 *     Set the color of broken (non-unified) tangent handles.
 *     Default: (0.545, 0.271, 0.075) - Dark Orange
 *     Example: cmds.tcMotionPathCmd(brokenTangentColor=[1.0, 0.0, 0.0])
 *
 * -wpc / -weightedPathColor <r> <g> <b>
 *     Set the color of weighted animation curve segments.
 *     Default: (1.0, 0.0, 0.0) - Red
 *     Example: cmds.tcMotionPathCmd(weightedPathColor=[0.0, 1.0, 0.0])
 *
 * -wtc / -weightedPathTangentColor <r> <g> <b>
 *     Set the color of weighted tangent handles.
 *     Default: (0.545, 0.0, 0.0) - Dark Red
 *     Example: cmds.tcMotionPathCmd(weightedPathTangentColor=[0.5, 0.0, 0.0])
 *
 * -bpc / -bufferPathColor <r> <g> <b>
 *     Set the color of buffer (ghost) paths.
 *     Default: (0.2, 0.2, 0.2) - Dark Gray
 *     Example: cmds.tcMotionPathCmd(bufferPathColor=[0.5, 0.5, 0.5])
 *
 * -fnc / -frameNumberColor <r> <g> <b>
 *     Set the color of frame number labels.
 *     Default: (0.098, 0.098, 0.098) - Almost Black
 *     Example: cmds.tcMotionPathCmd(frameNumberColor=[1.0, 1.0, 1.0])
 *
 * -knc / -keyframeNumberColor <r> <g> <b>
 *     Set the color of keyframe number labels.
 *     Default: (1.0, 1.0, 0.0) - Yellow
 *     Example: cmds.tcMotionPathCmd(keyframeNumberColor=[1.0, 0.5, 0.0])
 *
 * =============================================================================
 * DRAWING CONTROL FLAGS
 * =============================================================================
 *
 * -mdm / -drawMode <int>
 *     Set the drawing space mode.
 *     0 = World Space (default) - Path in world coordinates
 *     1 = Camera Space - Path relative to camera
 *     Example: cmds.tcMotionPathCmd(drawMode=1)
 *
 * -dti / -drawTimeInterval <double>
 *     Set the time interval for path sampling/drawing.
 *     Smaller values = smoother curves, higher performance cost.
 *     Default: 0.1
 *     Example: cmds.tcMotionPathCmd(drawTimeInterval=0.05)
 *
 * -fi / -frameInterval <int>
 *     Set the interval for displaying frame numbers.
 *     Shows frame numbers every N frames.
 *     Default: 5
 *     Example: cmds.tcMotionPathCmd(frameInterval=10)
 *
 * -sm / -strokeMode <int>
 *     Set the stroke drawing mode (for draw context).
 *     0 = Closest - Snap keyframes to nearest stroke point
 *     1 = Spread - Distribute keyframes evenly along stroke
 *     Example: cmds.tcMotionPathCmd(strokeMode=0)
 *
 * -dkc / -drawKeyframeCount <int>
 *     Set the number of keyframes to create when drawing paths.
 *     Keyframes are distributed evenly along the drawn path.
 *     Default: 5
 *     Example: cmds.tcMotionPathCmd(drawKeyframeCount=10)
 *
 * =============================================================================
 * BUFFER PATH FLAGS
 * =============================================================================
 *
 * -abp / -addBufferPaths
 *     Add current motion paths to buffer (ghost paths) for comparison.
 *     Example: cmds.tcMotionPathCmd(addBufferPaths=True)
 *
 * -dbs / -deleteAllBufferPaths
 *     Delete all buffer paths from memory.
 *     Example: cmds.tcMotionPathCmd(deleteAllBufferPaths=True)
 *
 * -dbi / -deleteBufferPathAtIndex <int>
 *     Delete a specific buffer path by index (0-based).
 *     Example: cmds.tcMotionPathCmd(deleteBufferPathAtIndex=0)
 *
 * -sbp / -selectBufferPathAtIndex <int>
 *     Select (show) a specific buffer path by index.
 *     Example: cmds.tcMotionPathCmd(selectBufferPathAtIndex=0)
 *
 * -dbp / -deselectBufferPathAtIndex <int>
 *     Deselect (hide) a specific buffer path by index.
 *     Example: cmds.tcMotionPathCmd(deselectBufferPathAtIndex=0)
 *
 * -cbp / -convertBufferPath <int>
 *     Convert a buffer path to a NURBS curve in the scene.
 *     Example: cmds.tcMotionPathCmd(convertBufferPath=0)
 *
 * =============================================================================
 * LOCKED MODE FLAGS
 * =============================================================================
 *
 * -l / -lockedMode <boolean>
 *     Enable locked selection mode.
 *     When enabled, motion path stays on selected objects even if selection changes.
 *     Creates world position callbacks for locked objects.
 *     Default: False
 *     Example: cmds.tcMotionPathCmd(lockedMode=True)
 *
 * -lmi / -lockedModeInteractive <boolean>
 *     Enable interactive updates in locked mode.
 *     When enabled, path updates during viewport manipulation.
 *     Default: False
 *     Example: cmds.tcMotionPathCmd(lockedModeInteractive=True)
 *
 * -rls / -refreshLockedSelection
 *     Force refresh of locked selection paths.
 *     Clears caches and recalculates paths for locked objects.
 *     Example: cmds.tcMotionPathCmd(refreshLockedSelection=True)
 *
 * =============================================================================
 * INTERNAL/UNDO FLAGS (typically not called directly by users)
 * =============================================================================
 *
 * -sdc / -storeDGAndCurveChange
 *     Internal: Store DG and animation curve changes for undo/redo.
 *     Used by edit contexts to prepare undoable operations.
 *
 * -ksc / -keySelectionChanged
 *     Internal: Handle keyframe selection change for undo/redo.
 *     Stores previous and current keyframe selection states.
 *
 * -sc / -selectionChanged
 *     Internal: Handle Maya selection change for undo/redo.
 *     Stores previous and new object selection.
 *
 * =============================================================================
 */

MSyntax MotionPathCmd::syntaxCreator()
{
    MSyntax syntax;

    // General control
    syntax.addFlag("-e", "-enable", MSyntax::kBoolean);
    syntax.addFlag("-gsl", "-getCurrentSL", MSyntax::kNoArg);
    syntax.addFlag("-rdt", "-refreshdt", MSyntax::kNoArg);

    // Time range
    syntax.addFlag("-bf", "-framesBefore",  MSyntax::kLong);
    syntax.addFlag("-af", "-framesAfter",  MSyntax::kLong);
    syntax.addFlag("-tfr", "-frameRange", MSyntax::kLong, MSyntax::kLong);

    // Display visibility
    syntax.addFlag("-st", "-showTangents", MSyntax::kBoolean);
    syntax.addFlag("-sp", "-showPath", MSyntax::kBoolean);
    syntax.addFlag("-sk", "-showKeyFrames", MSyntax::kBoolean);
    syntax.addFlag("-srk", "-showRotationKeyFrames", MSyntax::kBoolean);
    syntax.addFlag("-skn", "-showKeyFrameNumbers", MSyntax::kBoolean);
    syntax.addFlag("-sfn", "-showFrameNumbers", MSyntax::kBoolean);

    // Display style
    syntax.addFlag("-alf", "-alternatingFrames", MSyntax::kBoolean);
    syntax.addFlag("-up", "-usePivots", MSyntax::kBoolean);

    // Buffer paths
    syntax.addFlag("-abp", "-addBufferPaths", MSyntax::kNoArg);
    syntax.addFlag("-dbs", "-deleteAllBufferPaths", MSyntax::kNoArg);
    syntax.addFlag("-dbi", "-deleteBufferPathAtIndex", MSyntax::kLong);
    syntax.addFlag("-sbp", "-selectBufferPathAtIndex", MSyntax::kLong);
    syntax.addFlag("-dbp", "-deselectBufferPathAtIndex", MSyntax::kLong);

    // Buffer path queries
    syntax.addFlag("-qbpc", "-queryBufferPathCount", MSyntax::kNoArg);
    syntax.addFlag("-qbpn", "-queryBufferPathName", MSyntax::kLong);

    // Size settings
    syntax.addFlag("-fs", "-frameSize", MSyntax::kDouble);
    syntax.addFlag("-ps", "-pathSize", MSyntax::kDouble);
    syntax.addFlag("-kls", "-keyframeLabelSize", MSyntax::kDouble);
    syntax.addFlag("-fls", "-frameLabelSize", MSyntax::kDouble);

    // Drawing control
    syntax.addFlag("-mdm", "-drawMode", MSyntax::kLong);

    // Color settings
    syntax.addFlag("-cfc", "-currentFrameColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-pc", "-pathColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-tc", "-tangentColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-btc", "-brokenTangentColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-bpc", "-bufferPathColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-wpc", "-weightedPathColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-wtc", "-weightedPathTangentColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-fnc", "-frameNumberColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    syntax.addFlag("-knc", "-keyframeNumberColor", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);

    // Drawing intervals
    syntax.addFlag("-dti", "-drawTimeInterval", MSyntax::kDouble);
    syntax.addFlag("-fi", "-frameInterval", MSyntax::kLong);
    syntax.addFlag("-sm", "-strokeMode", MSyntax::kLong);
    syntax.addFlag("-dkc", "-drawKeyframeCount", MSyntax::kLong);

    // Internal/Undo flags
    syntax.addFlag("-sdc", "-storeDGAndCurveChange", MSyntax::kNoArg);
    syntax.addFlag("-cbp", "-convertBufferPath", MSyntax::kLong);
    syntax.addFlag("-ksc", "-keySelectionChanged", MSyntax::kNoArg);
    syntax.addFlag("-sc", "-selectionChanged", MSyntax::kNoArg);

    // Locked mode
    syntax.addFlag("-l", "-lockedMode", MSyntax::kBoolean);
    syntax.addFlag("-lmi", "-lockedModeInteractive", MSyntax::kBoolean);
    syntax.addFlag("-rls", "-refreshLockedSelection", MSyntax::kNoArg);

    syntax.useSelectionAsDefault(false);
    syntax.setObjectType(MSyntax::kSelectionList, 0);

	return syntax;
}

MStatus MotionPathCmd::doIt(const MArgList& args)
{
    MArgDatabase argData(syntax(), args);
    
	if(argData.isFlagSet("-enable"))
	{
		bool enable;
        argData.getFlagArgument("-enable", 0, enable);

		if(enable)
		{           
			mpManager.setupViewports();
			mpManager.addCallbacks();

            MotionPathManager::selectionChangeCallback(&mpManager);
		}
		else
		{
			mpManager.cleanupViewports();
			mpManager.removeCallbacks();
		}
        
        GlobalSettings::enabled = enable;
	}
	else if(argData.isFlagSet("-getCurrentSL"))
	{
		this->setResult(mpManager.getSelectionList());
	}
	else if(argData.isFlagSet("-frameRange"))
	{
        int initialFrame, lastFrame;
        argData.getFlagArgument("-frameRange", 0, initialFrame);
        argData.getFlagArgument("-frameRange", 1, lastFrame);
		mpManager.setTimeRange(initialFrame, lastFrame);
	}
	else if(argData.isFlagSet("-framesBefore"))
	{
		int beforeFrame;
        argData.getFlagArgument("-framesBefore", 0, beforeFrame);

        if (beforeFrame < 0)
            beforeFrame = 0;
        
        GlobalSettings::framesBack = beforeFrame;
	}
    else if(argData.isFlagSet("-framesAfter"))
	{
		int afterFrame;
        argData.getFlagArgument("-framesAfter", 0, afterFrame);
        
        if (afterFrame < 0)
            afterFrame = 0;
        
        GlobalSettings::framesFront = afterFrame;
	}
	else if(argData.isFlagSet("-rdt"))
	{
		mpManager.refreshDisplayTimeRange();
	}
    else if(argData.isFlagSet("-showTangents"))
    {
        bool showTangents;
        argData.getFlagArgument("-showTangents", 0, showTangents);
        GlobalSettings::showTangents = showTangents;
    }
    else if(argData.isFlagSet("-showKeyFrames"))
    {
        bool showKeyFrames;
        argData.getFlagArgument("-showKeyFrames", 0, showKeyFrames);
        GlobalSettings::showKeyFrames = showKeyFrames;
    }
    else if(argData.isFlagSet("-showPath"))
    {
        bool showPath;
        argData.getFlagArgument("-showPath", 0, showPath);
        GlobalSettings::showPath = showPath;
    }
    else if (argData.isFlagSet("-showRotationKeyFrames"))
    {
        bool showRotationKeyFrames;
        argData.getFlagArgument("-showRotationKeyFrames", 0, showRotationKeyFrames);
        GlobalSettings::showRotationKeyFrames = showRotationKeyFrames;
    }
    else if (argData.isFlagSet("-showKeyFrameNumbers"))
    {
        bool showKeyFrameNumbers;
        argData.getFlagArgument("-showKeyFrameNumbers", 0, showKeyFrameNumbers);
        GlobalSettings::showKeyFrameNumbers = showKeyFrameNumbers;
    }
    else if (argData.isFlagSet("-showFrameNumbers"))
    {
        bool showFrameNumbers;
        argData.getFlagArgument("-showFrameNumbers", 0, showFrameNumbers);
        GlobalSettings::showFrameNumbers = showFrameNumbers;
    }
    else if (argData.isFlagSet("-alternatingFrames"))
    {
        bool alternatingFrames;
        argData.getFlagArgument("-alternatingFrames", 0, alternatingFrames);
        GlobalSettings::alternatingFrames = alternatingFrames;
    }
    else if (argData.isFlagSet("-usePivots"))
    {
        bool usePivots;
        argData.getFlagArgument("-usePivots", 0, usePivots);
        GlobalSettings::usePivots = usePivots;
        
        mpManager.clearParentMatrixCaches();
        mpManager.refreshDisplayTimeRange();
    }
    else if (argData.isFlagSet("-pathSize"))
    {
        double pathSize;
        argData.getFlagArgument("-pathSize", 0, pathSize);
        GlobalSettings::pathSize = pathSize;
    }
    else if (argData.isFlagSet("-frameSize"))
    {
        double frameSize;
        argData.getFlagArgument("-frameSize", 0, frameSize);
        GlobalSettings::frameSize = frameSize;
    }
    else if (argData.isFlagSet("-keyframeLabelSize"))
    {
        double keyframeLabelSize;
        argData.getFlagArgument("-keyframeLabelSize", 0, keyframeLabelSize);
        GlobalSettings::keyframeLabelSize = keyframeLabelSize;
        mpManager.refreshDisplayTimeRange();
    }
    else if (argData.isFlagSet("-frameLabelSize"))
    {
        double frameLabelSize;
        argData.getFlagArgument("-frameLabelSize", 0, frameLabelSize);
        GlobalSettings::frameLabelSize = frameLabelSize;
        mpManager.refreshDisplayTimeRange();
    }
    else if (argData.isFlagSet("-drawTimeInterval"))
    {
        double drawTimeInterval;
        argData.getFlagArgument("-drawTimeInterval", 0, drawTimeInterval);
        GlobalSettings::drawTimeInterval = drawTimeInterval;

        // Refresh display to apply new time sampling interval for path smoothness
        // The drawFrames() loop uses this value to control path density
        mpManager.refreshDisplayTimeRange();
    }
    else if (argData.isFlagSet("-strokeMode"))
    {
        int strokeMode;
        argData.getFlagArgument("-strokeMode", 0, strokeMode);
        GlobalSettings::strokeMode = strokeMode;
    }
    else if (argData.isFlagSet("-drawMode"))
    {
        int drawMode;
        argData.getFlagArgument("-drawMode", 0, drawMode);
        
        if (GlobalSettings::motionPathDrawMode != drawMode)
        {
            /*
            if (drawMode == 0)
                mpManager.destroyCameraCachesAndCameraCallbacks();
            else
                mpManager.createCameraCachesAndCameraCallbacks();
            */
            
            if (drawMode < 0)
                drawMode = 0;

            if (drawMode > 1)
                drawMode = 1;

            mpManager.cacheCameras();
            GlobalSettings::motionPathDrawMode = (GlobalSettings::DrawMode) drawMode;

            // Trigger viewport refresh when draw mode changes
            MGlobal::executeCommandOnIdle("refresh");
        }
    }
    else if (argData.isFlagSet("-frameInterval"))
    {
        int frameInterval;
        argData.getFlagArgument("-frameInterval", 0, frameInterval);
        GlobalSettings::drawFrameInterval = frameInterval;

        // Refresh display to apply new frame label interval
        // The drawFrameLabels() loop uses this value to control label display frequency
        mpManager.refreshDisplayTimeRange();
    }
    else if (argData.isFlagSet("-drawKeyframeCount"))
    {
        int keyframeCount;
        argData.getFlagArgument("-drawKeyframeCount", 0, keyframeCount);
        if (keyframeCount > 0)
        {
            GlobalSettings::drawKeyframeCount = keyframeCount;
        }
    }
    else if (argData.isFlagSet("-currentFrameColor"))
    {
        GlobalSettings::currentFrameColor = getColorFromArg(argData, "-currentFrameColor");
    }
    else if (argData.isFlagSet("-pathColor"))
    {
        GlobalSettings::pathColor = getColorFromArg(argData, "-pathColor");
    }
    else if (argData.isFlagSet("-tangentColor"))
    {
        GlobalSettings::tangentColor = getColorFromArg(argData, "-tangentColor");
    }
	else if (argData.isFlagSet("-brokenTangentColor"))
    {
        GlobalSettings::brokenTangentColor = getColorFromArg(argData, "-brokenTangentColor");
    }
    else if (argData.isFlagSet("-bufferPathColor"))
    {
        GlobalSettings::bufferPathColor = getColorFromArg(argData, "-bufferPathColor");
    }
    else if (argData.isFlagSet("-weightedPathColor"))
    {
        GlobalSettings::weightedPathColor = getColorFromArg(argData, "-weightedPathColor");
    }
    else if (argData.isFlagSet("-weightedPathTangentColor"))
    {
        GlobalSettings::weightedPathTangentColor = getColorFromArg(argData, "-weightedPathTangentColor");
    }
    else if (argData.isFlagSet("-frameNumberColor"))
    {
        GlobalSettings::frameLabelColor = getColorFromArg(argData, "-frameNumberColor");
    }
    else if (argData.isFlagSet("-keyframeNumberColor"))
    {
        GlobalSettings::keyframeLabelColor = getColorFromArg(argData, "-keyframeNumberColor");
    }
    else if (argData.isFlagSet("-addBufferPaths"))
    {
        mpManager.addBufferPaths();
    }
    else if (argData.isFlagSet("-deleteAllBufferPaths"))
    {
        mpManager.deleteAllBufferPaths();
    }
    else if (argData.isFlagSet("-deleteBufferPathAtIndex"))
    {
        int index;
        argData.getFlagArgument("-deleteBufferPathAtIndex", 0, index);
        mpManager.deleteBufferPathAtIndex(index);
    }
    else if (argData.isFlagSet("-selectBufferPathAtIndex"))
    {
        int index;
        argData.getFlagArgument("-selectBufferPathAtIndex", 0, index);
        mpManager.setSelectStateForBufferPathAtIndex(index, true);
    }
    else if (argData.isFlagSet("-deselectBufferPathAtIndex"))
    {
        int index;
        argData.getFlagArgument("-deselectBufferPathAtIndex", 0, index);
        mpManager.setSelectStateForBufferPathAtIndex(index, false);
    }
    else if (argData.isFlagSet("-queryBufferPathCount"))
    {
        int count = mpManager.getBufferPathCount();
        setResult(count);
        return MS::kSuccess;
    }
    else if (argData.isFlagSet("-queryBufferPathName"))
    {
        int index;
        argData.getFlagArgument("-queryBufferPathName", 0, index);
        BufferPath* bp = mpManager.getBufferPathAtIndex(index);
        if (bp)
        {
            setResult(bp->getObjectName());
        }
        else
        {
            setResult("");
        }
        return MS::kSuccess;
    }
    else if(argData.isFlagSet("-lockedMode"))
	{
        bool value;
        argData.getFlagArgument("-lockedMode", 0, value);
        GlobalSettings::lockedMode = value;
        if (value)
            mpManager.createMotionPathWorldCallback();
        else
            mpManager.destroyMotionPathWorldCallback();
	}
    else if(argData.isFlagSet("-lockedModeInteractive"))
	{
        bool lockedModeInteractive;
        argData.getFlagArgument("-lockedModeInteractive", 0, lockedModeInteractive);
        GlobalSettings::lockedModeInteractive = lockedModeInteractive;
	}
    else if(argData.isFlagSet("-refreshLockedSelection"))
	{
        mpManager.clearParentMatrixCaches();
        mpManager.refreshDisplayTimeRange();
	}
	else if (argData.isFlagSet("-storeDGAndCurveChange"))
	{
        dgModifierPtr = mpManager.getDGModifierPtr();
        animCurveChangePtr = mpManager.getAnimCurveChangePtr();

		if(dgModifierPtr)
			dgUndoable = true;
		else
			dgModifierPtr = NULL;
        
        if(animCurveChangePtr)
			animUndoable = true;
		else
			animCurveChangePtr = NULL;
	}
    else if (argData.isFlagSet("-convertBufferPath"))
    {
        int index;
        argData.getFlagArgument("-convertBufferPath", 0, index);
        BufferPath *bp = mpManager.getBufferPathAtIndex(index);
        
        if (bp == NULL)
        {
            MGlobal::displayError("tcMotionPathCmd: wrong buffer path index given.");
            return MS::kFailure;
        }
        
        mpManager.startDGUndoRecording();
        if (!createCurveFromBufferPath(bp))
        {
            mpManager.stopDGAndAnimUndoRecording();
            MGlobal::displayError("tcMotionPathCmd: could not convert curve.");
            return MS::kFailure;
        }
        mpManager.stopDGAndAnimUndoRecording();
    }
    else if (argData.isFlagSet("-keySelectionChanged"))
    {
        keySelectionUndoable = true;
        mpManager.getPreviousKeySelection(initialSelection);
        mpManager.getCurrentKeySelection(finalSelection);
        return redoIt();
    }
    else if(argData.isFlagSet("-selectionChanged"))
	{
        if (argData.getObjects(newSelection) == MS::kFailure)
        {
            MGlobal::displayError("tcMotionPathCmd: failed while parsing arguments");
            return MS::kFailure;
        }
        
        selectionUndoable = true;
        mpManager.getPreviousKeySelection(initialSelection);
        MGlobal::getActiveSelectionList(oldSelection);
        return redoIt();
    }
	else
	{
		MGlobal::displayError("tcMotionPathCmd: wrong flag.");
        return MS::kFailure;
	}

	return MS::kSuccess;
}

MStatus MotionPathCmd::redoIt()
{
    if(animUndoable && animCurveChangePtr)
		animCurveChangePtr->redoIt();
    
    if (dgUndoable && dgModifierPtr)
        dgModifierPtr->doIt();
    
    if (keySelectionUndoable)
    {
        if (GlobalSettings::enabled)
        {
            restoreKeySelection(finalSelection);
            mpManager.storePreviousKeySelection();
        }
    }
    
    if (selectionUndoable)
        MGlobal::setActiveSelectionList(newSelection);
    
	return MS::kSuccess;
}

MStatus MotionPathCmd::undoIt()
{
	if(animUndoable && animCurveChangePtr)
        animCurveChangePtr->undoIt();
    
    if (dgUndoable && dgModifierPtr)
        dgModifierPtr->undoIt();
    
    if (keySelectionUndoable)
    {
        if (GlobalSettings::enabled)
        {
            restoreKeySelection(initialSelection);
            mpManager.storePreviousKeySelection();
        }
    }
    
    if (selectionUndoable)
    {
        MGlobal::setActiveSelectionList(oldSelection);
        if (GlobalSettings::enabled)
        {
            restoreKeySelection(initialSelection);
            mpManager.storePreviousKeySelection();
        }
    }

	return MS::kSuccess;
}

void MotionPathCmd::restoreKeySelection(const std::vector<MDoubleArray> &sel)
{
    for (int i = 0; i < sel.size(); ++i)
    {
        MotionPath *motionPathPtr = mpManager.getMotionPathPtr(i);
        if (motionPathPtr)
        {
            motionPathPtr->deselectAllKeys();
            for (int j = 0; j < sel[i].length(); ++j)
                motionPathPtr->selectKeyAtTime(sel[i][j]);
        }
    }
    
    M3dView::active3dView().refresh();
}

bool MotionPathCmd::createCurveFromBufferPath(BufferPath *bp)
{
    const std::vector<MVector>* points = bp->getFrames();
    if (points == NULL) return false;

    // Build curve name from object name with "_Buffer_Path" suffix
    MString curveName = bp->getObjectName();
    if (curveName.length() == 0)
        curveName = "BufferPath";
    curveName += "_Buffer_Path";

    // Build MEL command with proper naming
    MString cmd = "curve -d 1 -name \"" + curveName + "\" ";
    MString cvsStr = "";
    for (unsigned int i = 0; i < points->size(); ++i)
    {
        MVector v = points->at(i);
        cvsStr += MString("-p ") + v.x + " " + v.y + " " + v.z + " ";
    }

    MString knotsStr = "";
    for (unsigned int i = 0; i < points->size(); i++)
        knotsStr += MString("-k ") + ((double) i) + " ";

    MDGModifier *dg = mpManager.getDGModifierPtr();
    dg->commandToExecute(cmd + cvsStr + knotsStr);
    return dg->doIt() == MS::kSuccess;
}

