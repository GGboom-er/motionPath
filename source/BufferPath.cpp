//
//  BufferPath.cpp
//  MotionPath
//
//  Created by Daniele Federico on 06/12/14.
//
//

#include "BufferPath.h"
#include "GlobalSettings.h"
// A1: Legacy GL removed - using VP2DrawUtils only
#include "Vp2DrawUtils.h"

BufferPath::BufferPath()
{
    black = MColor(0,0,0);
    selected = false;
}

void BufferPath::drawFrames(const double startTime, const double endTime, const MColor &curveColor, CameraCache* cachePtr, const MMatrix &currentCameraMatrix, M3dView &view, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext)
{
    int frameSize = static_cast<int>(frames.size());

    if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
    {
        if (!cachePtr) return;
        cachePtr->ensureMatricesAtTime(startTime);
    }

    // Performance optimization: Collect vertices for batch drawing
    std::vector<MVector> lineVertices;
    std::vector<MVector> pointVertices;

    if (GlobalSettings::showPath)
        lineVertices.reserve(static_cast<size_t>((endTime - startTime + 1) * 2));
    pointVertices.reserve(static_cast<size_t>(endTime - startTime + 2));

	for(double i = startTime + 1; i <= endTime; i += 1.0)
	{
        if (i <= minTime || i >= minTime + frameSize)
            continue;

        int index = static_cast<int>(i - minTime);

        MVector pos1 = frames[index];
        MVector pos2 = frames[index-1];
        if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
        {
            cachePtr->ensureMatricesAtTime(i);
            pos1 = MPoint(pos1) * cachePtr->matrixCache[CameraCache::timeToTick(i)] * currentCameraMatrix;
            pos2 = MPoint(pos2) * cachePtr->matrixCache[CameraCache::timeToTick(i-1)] * currentCameraMatrix;
        }

		if (GlobalSettings::showPath)
		{
            // For VP2, still draw individually (VP2 has its own batching)
			VP2DrawUtils::drawLineWithColor(pos1, pos2, static_cast<float>(GlobalSettings::pathSize), curveColor, currentCameraMatrix, drawManager, frameContext);
		}

		VP2DrawUtils::drawPointWithColor(pos2, static_cast<float>(GlobalSettings::frameSize), curveColor, currentCameraMatrix, drawManager, frameContext);

        if (i == endTime || i == minTime + frameSize - 1)
		{
			VP2DrawUtils::drawPointWithColor(pos1, static_cast<float>(GlobalSettings::frameSize), curveColor, currentCameraMatrix, drawManager, frameContext);
		}
	}

    // Batch draw for legacy OpenGL
    // VP2-only: Legacy OpenGL path removed
}

void BufferPath::drawKeyFrames(const double startTime, const double endTime, const MColor &curveColor, CameraCache* cachePtr, const MMatrix &currentCameraMatrix, M3dView &view, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext)
{
    for(BPKeyframeIterator keyIt = keyFrames.begin(); keyIt != keyFrames.end(); ++keyIt)
    {
        double time = keyIt->first;
        if (time >= startTime && time <= endTime)
        {
            MVector pos = keyIt->second;
            if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
            {
                if (!cachePtr) continue;
                cachePtr->ensureMatricesAtTime(time);
                pos = MPoint(pos) * cachePtr->matrixCache[CameraCache::timeToTick(time)] * currentCameraMatrix;
            }

			VP2DrawUtils::drawPointWithColor(pos, static_cast<float>(GlobalSettings::frameSize), curveColor, GlobalSettings::cameraMatrix, drawManager, frameContext);
        }
    }
}

void BufferPath::draw(M3dView &view, CameraCache* cachePtr, MHWRender::MUIDrawManager* drawManager, const MHWRender::MFrameContext* frameContext)
{
    MColor curveColor = GlobalSettings::bufferPathColor;
    
    if(selected)
    {
        curveColor.r = 1.0f - curveColor.r;
        curveColor.g = 1.0f - curveColor.g;
        curveColor.b = 1.0f - curveColor.b;
    }

    curveColor.a = 0.5f;
    
    double currentTime = MAnimControl::currentTime().as(MTime::uiUnit());
    
    double startTime = currentTime - GlobalSettings::framesBack;
    double endTime = currentTime + GlobalSettings::framesFront;
    
    MMatrix currentCameraMatrix;
    if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
    {
        if (!cachePtr) return;
        double currentTime = MAnimControl::currentTime().as(MTime::uiUnit());
        cachePtr->ensureMatricesAtTime(currentTime);
        currentCameraMatrix = cachePtr->matrixCache[CameraCache::timeToTick(currentTime)].inverse();
    }
    
    drawFrames(startTime, endTime, curveColor, cachePtr, GlobalSettings::cameraMatrix, view, drawManager, frameContext);
    if (GlobalSettings::showKeyFrames)
        drawKeyFrames(startTime, endTime, curveColor, cachePtr, GlobalSettings::cameraMatrix, view, drawManager, frameContext);
    
    //draw current frame
    if (currentTime >= minTime && currentTime <= minTime + frames.size())
    {
        MColor currentColor = GlobalSettings::currentFrameColor * 0.8f;
        currentColor.a = 0.7f;
        
		int numFrame = static_cast<int>(currentTime) - static_cast<int>(minTime);
		if (numFrame < 0 || numFrame > frames.size() - 1)
			return;

        MVector pos = frames[static_cast<int>(currentTime) - static_cast<int>(minTime)];
        if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
        {
            cachePtr->ensureMatricesAtTime(currentTime);
            pos = MPoint(pos) * cachePtr->matrixCache[CameraCache::timeToTick(currentTime)] * currentCameraMatrix;
        }
        
		VP2DrawUtils::drawPointWithColor(pos, static_cast<float>(GlobalSettings::frameSize), curveColor, GlobalSettings::cameraMatrix, drawManager, frameContext);
    }
}
