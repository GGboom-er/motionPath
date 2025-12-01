#pragma once

#include <maya/M3dView.h>
#include <maya/MVector.h>
#include <maya/MColor.h>
#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MGlobal.h>
#include <maya/MViewport2Renderer.h>

#include <set>

#include <Keyframe.h>
#include <CameraCache.h>

namespace VP2DrawUtils
{
	void drawLineStipple(const MVector &origin, const MVector &target, float lineWidth, const MColor &color, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void drawLine(const MVector &origin, const MVector &target, float lineWidth, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void drawLineWithColor(const MVector &origin, const MVector &target, float lineWidth, const MColor &color, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void drawPoint(const MVector &point, float size, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void drawPointWithColor(const MVector &point, float size, const MColor &color, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void drawKeyFramePoints(KeyframeMap &keyframesCache, const float size, const double colorMultiplier, const int portWidth, const int portHeight, const bool showRotationKeyframes, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void drawKeyFrames(std::vector<Keyframe *> keys, const float size, const double colorMultiplier, const int portWidth, const int portHeight, const bool showRotationKeyframes, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void convertWorldSpaceToCameraSpace(CameraCache* cachePtr, std::map<double, MPoint> &positions, std::map<double, MPoint> &screenSpacePositions, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	void drawFrameLabel(double frame, const MVector &framePos, M3dView &view, const double sizeOffset, const MColor &color, const MMatrix &cameraMatrix, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext);

	// A3: Cached screen-space drawing functions (avoid worldToViewport calls)
	void drawLine2dCached(const MPoint &screenPos1, const MPoint &screenPos2, float lineWidth, const MColor &color, MHWRender::MUIDrawManager* drawManager);
	void drawPoint2dCached(const MPoint &screenPos, float size, const MColor &color, MHWRender::MUIDrawManager* drawManager);

	// B4: Cached label drawing (avoids worldToViewport)
	void drawFrameLabelCached(double frame, const MPoint &screenPos, const double sizeOffset, const MColor &color, MHWRender::MUIDrawManager* drawManager);
}
