//
//  ContextUtils.h
//  MotionPath
//
//  Created by Daniele Federico on 29/11/14.
//
//

#ifndef CONTEXTUTILS_H
#define CONTEXTUTILS_H

#include "MotionPathManager.h"
#include "MotionPath.h"

#include <maya/MVector.h>
#include <maya/M3dView.h>

namespace contextUtils
{
    bool worldCameraSpaceToWorldSpace(MVector &position, M3dView &view, const double time, const MMatrix &inverseCameraMatrix, MotionPathManager &mpManager);
    bool worldCameraSpaceToWorldSpace(MPoint &position, M3dView &view, const double time, const MMatrix &inverseCameraMatrix, MotionPathManager &mpManager);

    // VP2: MFrameContext-based coordinate conversion (replaces M3dView version)
    MVector getWorldPositionFromProjPointVP2(const MVector &pointToMove, const double initialX, const double initialY, const double currentX, const double currentY, const MHWRender::MFrameContext &frameContext, const MVector &cameraPosition);

    // Legacy: M3dView-based (deprecated, for compatibility only)
    MVector getWorldPositionFromProjPoint(const MVector &pointToMove, const double initialX, const double initialY, const double currentX, const double currentY, const M3dView &view, const MVector &cameraPosition);

    // A1: DEPRECATED - GL selection buffer versions removed, use coordinate-based versions below
    /*
    int processCurveHits(M3dView &view, CameraCache *cachePtr, MotionPathManager &mpManager);
    void processTangentHits(MotionPath* motionPathPtr, M3dView &view, CameraCache *cachePtr, int &selectedKeyId, int &selectedTangent);
    void processKeyFrameHits(MotionPath* motionPathPtr, M3dView &view, CameraCache *cachePtr, MIntArray &selectedKeys);
    bool processFramesHits(MotionPath* motionPathPtr, M3dView &view, CameraCache *cachePtr, double &time);
    */

	int processCurveHits(const short mx, const short my, const MMatrix &cameraMatrix, M3dView &view, CameraCache *cachePtr, MotionPathManager &mpManager);
	void processTangentHits(const short mx, const short my, MotionPath* motionPathPtr, M3dView &view, const MMatrix &cameraMatrix, CameraCache *cachePtr, int &selectedKeyId, int &selectedTangent);
	void processKeyFrameHits(const short mx, const short my, MotionPath* motionPathPtr, M3dView &view, const MMatrix &cameraMatrix, CameraCache *cachePtr, MIntArray &selectedKeys);
	bool processFramesHits(const short mx, const short my, MotionPath* motionPathPtr, M3dView &view, const MMatrix &cameraMatrix, CameraCache *cachePtr, double &time);

    void refreshSelectionMethod(MEvent &event, MGlobal::ListAdjustment &listAdjustment);
    // A1: DEPRECATED - GL immediate mode removed, use VP2 version below
    // void drawMarqueeGL(short initialX, short initialY, short finalX, short finalY);
    void drawMarquee(MHWRender::MUIDrawManager& drawMgr, short initialX, short initialY, short finalX, short finalY);
    void applySelection(short initialX, short initialY, short finalX, short finalY, const MGlobal::ListAdjustment &listAdjustment);
    
}

#endif
