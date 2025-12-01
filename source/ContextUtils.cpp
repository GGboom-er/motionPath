//
//  ContextUtils.cpp
//  MotionPath
//
//  Created by Daniele Federico on 29/11/14.
//
//

#include "ContextUtils.h"
#include "Keyframe.h"
#include <maya/MPoint.h>
#include <maya/MIntArray.h>
#include <limits>

bool contextUtils::worldCameraSpaceToWorldSpace(MVector &position, M3dView &view, const double time, const MMatrix &inverseCameraMatrix, MotionPathManager &mpManager)
{
    CameraCache * cachePtr = mpManager.getCameraCachePtrFromView(view);
    if (!cachePtr)
        return false;
    cachePtr->ensureMatricesAtTime(time);
    position = position * inverseCameraMatrix * cachePtr->matrixCache[CameraCache::timeToTick(time)].inverse();
    return true;
}

bool contextUtils::worldCameraSpaceToWorldSpace(MPoint &position, M3dView &view, const double time, const MMatrix &inverseCameraMatrix, MotionPathManager &mpManager)
{
    CameraCache * cachePtr = mpManager.getCameraCachePtrFromView(view);
    if (!cachePtr)
        return false;
    cachePtr->ensureMatricesAtTime(time);
    position = position * inverseCameraMatrix * cachePtr->matrixCache[CameraCache::timeToTick(time)].inverse();
    return true;
}

// VP2: MFrameContext-based coordinate conversion (correct viewport handling)
MVector contextUtils::getWorldPositionFromProjPointVP2(const MVector &pointToMove, const double initialX, const double initialY, const double currentX, const double currentY, const MHWRender::MFrameContext &frameContext, const MVector &cameraPosition)
{
    MPoint startWorldNear, startWorldFar;
    MPoint endWorldNear, endWorldFar;

    // Convert viewport coordinates to world space rays
    frameContext.viewportToWorld(initialX, initialY, startWorldNear, startWorldFar);
    frameContext.viewportToWorld(currentX, currentY, endWorldNear, endWorldFar);

    // Calculate ray directions
    MVector startRayDir = MVector(startWorldFar - startWorldNear).normal();
    MVector endRayDir = MVector(endWorldFar - endWorldNear).normal();

    // Calculate distance from camera to the point we're moving
    double distanceToCamera = (pointToMove - cameraPosition).length();

    // Project along rays at the same distance as the original point
    MPoint startPoint = startWorldNear + startRayDir * distanceToCamera;
    MPoint endPoint = endWorldNear + endRayDir * distanceToCamera;

    // Calculate the offset
    return MVector(endPoint - startPoint) + pointToMove;
}

// Legacy: M3dView-based (deprecated, for compatibility only)
MVector contextUtils::getWorldPositionFromProjPoint(const MVector &pointToMove, const double initialX, const double initialY, const double currentX, const double currentY, const M3dView &view, const MVector &cameraPosition)
{
    MPoint startPoint, endPoint;
	MVector worldVectorStart, worldVectorEnd;

	view.viewToWorld(initialX, initialY, startPoint, worldVectorStart);
	view.viewToWorld(currentX, currentY, endPoint, worldVectorEnd);

	double distanceToCamera = (pointToMove - cameraPosition).length();

	startPoint += worldVectorStart * distanceToCamera;
	endPoint += worldVectorEnd * distanceToCamera;

    return (endPoint - startPoint) + pointToMove;
}

// A1: DEPRECATED - GL selection buffer removed (replaced by coordinate-based picking)
// All callers now use processCurveHits(mx, my, ...) version which uses cached screen positions
/*
int contextUtils::processCurveHits(M3dView &view, CameraCache *cachePtr, MotionPathManager &mpManager)
{
    GLuint glSelectionBuffer[256];
    view.beginSelect(glSelectionBuffer, 256);
    view.initNames();

    mpManager.drawCurvesForSelection(view, cachePtr);

    GLint hits = view.endSelect();
    if (hits == 0)  return -1;

	GLuint* ptr = (GLuint *) glSelectionBuffer;
    ptr += (hits - 1) * 4 + 3;
	return *ptr;
}
*/

int contextUtils::processCurveHits(const short mx, const short my, const MMatrix &cameraMatrix, M3dView &view, CameraCache *cachePtr, MotionPathManager &mpManager)
{
	for (int i = 0; i < mpManager.getMotionPathsCount(); ++i)
	{
		MotionPath* mpPtr = mpManager.getMotionPathPtr(i);
		if (!mpPtr)
			continue;

		std::vector<std::pair<int, MVector>> vec;
		
		MIntArray keys;
		contextUtils::processKeyFrameHits(mx, my, mpPtr, view, cameraMatrix, cachePtr, keys);
		if (keys.length() > 0)
			return i;

		int selectedKeyId, selectedTangent;
		contextUtils::processTangentHits(mx, my, mpPtr, view, cameraMatrix, cachePtr, selectedKeyId, selectedTangent);
		if (selectedTangent != -1)
			return i;

		double time;
		if (contextUtils::processFramesHits(mx, my, mpPtr, view, cameraMatrix, cachePtr, time))
			return i;
	}
	return -1;
}

void contextUtils::processKeyFrameHits(const short mx, const short my, MotionPath* motionPathPtr, M3dView &view, const MMatrix &cameraMatrix, CameraCache *cachePtr, MIntArray &selectedKeys)
{
    KeyframeMap *km = motionPathPtr->keyFramesCachePtr();
    int key = -1;
    double bestDistance = std::numeric_limits<double>::max();

    double kfs = GlobalSettings::frameSize * 1.5 / 2;
    kfs = kfs * kfs;

    const bool useBuckets = motionPathPtr->hasKeyBuckets();
    if (useBuckets)
    {
        int bx = motionPathPtr->bucketX(mx);
        int by = motionPathPtr->bucketY(my);
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                int64_t bkey = MotionPath::bucketKey(bx + dx, by + dy);
                const auto& buckets = motionPathPtr->getKeyBuckets();
                auto itBucket = buckets.find(bkey);
                if (itBucket == buckets.end())
                    continue;

                for (Keyframe* kPtr : itBucket->second)
                {
                    if (!kPtr)
                        continue;
                    double x = kPtr->projPosition.x;
                    double y = kPtr->projPosition.y;
                    double distance = (mx - x) * (mx - x) + (my - y) * (my - y);
                    if (distance < kfs && distance < bestDistance)
                    {
                        bestDistance = distance;
                        key = kPtr->id;
                    }
                }
            }
        }
    }

    if (key == -1)
    {
        for (KeyframeMapIterator it = km->begin(); it != km->end(); ++it)
        {
            Keyframe &k = it->second;

            // A3: Use cached screen-space coordinates (projPosition) if available
            // This avoids O(N) worldToView calls during picking
            double x, y;
            if (k.projPosition.x != 0.0 || k.projPosition.y != 0.0 || k.worldPosition.length() < 0.001)
            {
                // Use cached screen position (already computed in drawKeyFrames)
                x = k.projPosition.x;
                y = k.projPosition.y;
            }
            else
            {
                // Fallback: compute on-the-fly if cache not available
                short sx, sy;
                view.worldToView(k.worldPosition, sx, sy);
                x = sx;
                y = sy;
            }

            double distance = (mx - x) * (mx - x) + (my - y) * (my - y);
            if (distance < kfs && distance < bestDistance)
            {
                bestDistance = distance;
                key = it->second.id;
            }
        }
    }

    if (key != -1)
        selectedKeys.append(key);
}

void contextUtils::processTangentHits(const short mx, const short my, MotionPath* motionPathPtr, M3dView &view, const MMatrix &cameraMatrix, CameraCache *cachePtr, int &selectedKeyId, int &selectedTangent)
{
	KeyframeMap *km = motionPathPtr->keyFramesCachePtr();

	double tfs = GlobalSettings::frameSize / 2;
	tfs = tfs * tfs;

	selectedTangent = -1;
    double bestDistance = std::numeric_limits<double>::max();

    const bool useBuckets = motionPathPtr->hasTangentBuckets();
    if (useBuckets)
    {
        int bx = motionPathPtr->bucketX(mx);
        int by = motionPathPtr->bucketY(my);
        const auto& buckets = motionPathPtr->getTangentBuckets();
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                int64_t bkey = MotionPath::bucketKey(bx + dx, by + dy);
                auto bucketIt = buckets.find(bkey);
                if (bucketIt == buckets.end())
                    continue;

                for (const auto& entry : bucketIt->second)
                {
                    if (!entry.key)
                        continue;

                    const MVector& proj = (entry.tangentId == Keyframe::kInTangent) ? entry.key->inTangentProj : entry.key->outTangentProj;
                    double distance = (mx - proj.x) * (mx - proj.x) + (my - proj.y) * (my - proj.y);
                    if (distance < tfs && distance < bestDistance)
                    {
                        bestDistance = distance;
                        selectedKeyId = entry.key->id;
                        selectedTangent = static_cast<int>(entry.tangentId);
                    }
                }
            }
        }
        if (selectedTangent != -1)
            return;
    }

	for (KeyframeMapIterator it = km->begin(); it != km->end(); ++it)
	{
		Keyframe &k = it->second;

		// A3: Use cached tangent screen positions if available (VP2 optimization)
		double inX, inY, outX, outY;
		if (k.inTangentProj.x != 0.0 || k.inTangentProj.y != 0.0)
		{
			// Use cached screen position for in-tangent
			inX = k.inTangentProj.x;
			inY = k.inTangentProj.y;
		}
		else
		{
			// Fallback: compute on-the-fly
			short sx, sy;
			view.worldToView(k.inTangentWorldFromCurve, sx, sy);
			inX = sx;
			inY = sy;
		}

		double distance = (mx - inX) * (mx - inX) + (my - inY) * (my - inY);
		if  (distance < tfs)
		{
            if (distance < bestDistance)
            {
                bestDistance = distance;
                selectedKeyId = k.id;
                selectedTangent = (int)Keyframe::kInTangent;
            }
		}

		if (k.outTangentProj.x != 0.0 || k.outTangentProj.y != 0.0)
		{
			// Use cached screen position for out-tangent
			outX = k.outTangentProj.x;
			outY = k.outTangentProj.y;
		}
		else
		{
			// Fallback: compute on-the-fly
			short sx, sy;
			view.worldToView(k.outTangentWorldFromCurve, sx, sy);
			outX = sx;
			outY = sy;
		}

		distance = (mx - outX) * (mx - outX) + (my - outY) * (my - outY);
		if (distance < tfs)
		{
            if (distance < bestDistance)
            {
                bestDistance = distance;
                selectedKeyId = k.id;
                selectedTangent = (int)Keyframe::kOutTangent;
            }
		}
	}
}

bool contextUtils::processFramesHits(const short mx, const short my, MotionPath* motionPathPtr, M3dView &view, const MMatrix &cameraMatrix, CameraCache *cachePtr, double &time)
{
	double fs = GlobalSettings::frameSize / 2;
	fs = fs * fs;
    double bestDistance = std::numeric_limits<double>::max();

    // A3/B1: Use cached screen-space positions and buckets if available
    const std::unordered_map<int64_t, MPoint>& screenCache = motionPathPtr->getFrameScreenPositions();
    const bool useBuckets = motionPathPtr->hasFrameBuckets();

    if (useBuckets && !screenCache.empty())
    {
        int bx = motionPathPtr->bucketX(mx);
        int by = motionPathPtr->bucketY(my);
        const auto& frameBuckets = motionPathPtr->getFrameBuckets();
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                int64_t bkey = MotionPath::bucketKey(bx + dx, by + dy);
                auto bucketIt = frameBuckets.find(bkey);
                if (bucketIt == frameBuckets.end())
                    continue;

                for (int64_t tick : bucketIt->second)
                {
                    auto posIt = screenCache.find(tick);
                    if (posIt == screenCache.end())
                        continue;
                    const MPoint& screenPos = posIt->second;
                    double distance = (mx - screenPos.x) * (mx - screenPos.x) + (my - screenPos.y) * (my - screenPos.y);
                    if (distance < fs && distance < bestDistance)
                    {
                        bestDistance = distance;
                        time = static_cast<double>(tick) / 6000.0;
                    }
                }
            }
        }
        if (bestDistance < std::numeric_limits<double>::max())
            return true;
    }

    if (!screenCache.empty())
    {
        // Use cached screen-space positions (fallback without buckets)
        for (auto it = screenCache.begin(); it != screenCache.end(); ++it)
        {
            const MPoint& screenPos = it->second;
            double distance = (mx - screenPos.x) * (mx - screenPos.x) + (my - screenPos.y) * (my - screenPos.y);
            if (distance < fs && distance < bestDistance)
            {
                bestDistance = distance;
                time = static_cast<double>(it->first) / 6000.0;
            }
        }
        if (bestDistance < std::numeric_limits<double>::max())
            return true;
    }

    // Fallback: compute screen positions on-the-fly
    std::vector<std::pair<int, MVector>> vec;
    motionPathPtr->getFramePositions(vec);
    for (unsigned int i = 0; i < vec.size(); ++i)
    {
        MVector& pos = vec[i].second;

        short x, y;
        view.worldToView(MPoint(pos.x, pos.y, pos.z), x, y);

        double distance = (mx - x) * (mx - x) + (my - y) * (my - y);
        if (distance < fs && distance < bestDistance)
        {
            bestDistance = distance;
            time = vec[i].first;
        }
    }

	return bestDistance < std::numeric_limits<double>::max();
}

// A1: DEPRECATED - GL selection buffer removed (replaced by coordinate-based picking)
// All callers now use processTangentHits(mx, my, ...) version which uses cached tangent positions
/*
void contextUtils::processTangentHits(MotionPath* motionPathPtr, M3dView &view, CameraCache *cachePtr, int &selectedKeyId, int &selectedTangent)
{
    GLuint glSelectionBuffer[256];
    GLint hits;

    view.beginSelect(glSelectionBuffer, 256);
    view.initNames();
    motionPathPtr->drawTangentsForSelection(view, cachePtr);

    hits = view.endSelect();

    if (hits == 0)
    {
        selectedTangent = -1;
        return;
    }

	GLuint* ptr = (GLuint *) glSelectionBuffer;
    ptr += (hits - 1) * 5 + 3;
    selectedKeyId = *ptr;
    ++ptr;
    selectedTangent = *ptr;
}
*/

// A1: DEPRECATED - GL selection buffer removed (replaced by coordinate-based picking)
// All callers now use processKeyFrameHits(mx, my, ...) version which uses cached key positions
/*
void contextUtils::processKeyFrameHits(MotionPath* motionPathPtr, M3dView &view, CameraCache *cachePtr, MIntArray &selectedKeys)
{
    GLuint glSelectionBuffer[256];
    GLint hits;

    view.beginSelect(glSelectionBuffer, 256);
    view.initNames();
    motionPathPtr->drawKeysForSelection(view, cachePtr);
    hits = view.endSelect();

    if (hits == 0)
        return;

    GLuint* ptr = (GLuint *) glSelectionBuffer;
    ptr += (hits - 1) * 4 + 1;
    GLuint minZ = *ptr;
    ptr += 2;
    selectedKeys.append(*ptr);

    for (int i = 0; i < hits - 1; ++i)
    {
        if (minZ == glSelectionBuffer[i * 4 + 1])
            selectedKeys.append(glSelectionBuffer[i * 4 + 3]);
    }
}
*/

// A1: DEPRECATED - GL selection buffer removed (replaced by coordinate-based picking)
// All callers now use processFramesHits(mx, my, ...) version which uses cached frame positions
/*
bool contextUtils::processFramesHits(MotionPath* motionPathPtr, M3dView &view, CameraCache *cachePtr, double &time)
{
    GLuint glSelectionBuffer[256];
    GLint hits;

    view.beginSelect(glSelectionBuffer, 256);
    view.initNames();
    motionPathPtr->drawFramesForSelection(view, cachePtr);

    hits = view.endSelect();

    if (hits == 0)
        return false;

    GLuint* ptr = (GLuint *) glSelectionBuffer;
    ptr += (hits - 1) * 4 + 3;
    time = *ptr;
	return true;
}
*/

// A1: DEPRECATED - GL immediate mode removed (replaced by VP2 MUIDrawManager)
// Use drawMarquee(MHWRender::MUIDrawManager&, ...) instead
/*
void contextUtils::drawMarqueeGL(short initialX, short initialY, short finalX, short finalY)
{
    glBegin( GL_LINE_LOOP );
    glVertex2i( initialX, initialY );
    glVertex2i( finalX, initialY );
    glVertex2i( finalX, finalY );
    glVertex2i( initialX, finalY );
    glEnd();
}
*/

void contextUtils::drawMarquee(MHWRender::MUIDrawManager& drawMgr, short initialX, short initialY, short finalX, short finalY)
{
    drawMgr.beginDrawable();
    
    drawMgr.line2d( MPoint( initialX, initialY), MPoint(finalX, initialY) );
    drawMgr.line2d( MPoint( finalX, initialY), MPoint(finalX, finalY) );
    drawMgr.line2d( MPoint( finalX, finalY), MPoint(initialX, finalY) );
    drawMgr.line2d( MPoint( initialX, finalY), MPoint(initialX, initialY) );
    
    drawMgr.endDrawable();
}


void contextUtils::applySelection(short initialX, short initialY, short finalX, short finalY, const MGlobal::ListAdjustment &listAdjustment)
{
    MSelectionList          incomingList, marqueeList;
    
    // Save the state of the current selections.  The "selectFromSceen"
    // below will alter the active list, and we have to be able to put
    // it back.
    MGlobal::getActiveSelectionList(incomingList);
    
    // If we have a zero dimension box, just do a point pick
    //
    if ( abs(initialX - finalX) < 2 && abs(initialY - finalY) < 2 ) {
        // This will check to see if the active view is in wireframe or not.
        MGlobal::SelectionMethod selectionMethod = MGlobal::selectionMethod();
        MGlobal::selectFromScreen( initialX, initialY, listAdjustment, selectionMethod );
    } else {
        // The Maya select tool goes to wireframe select when doing a marquee, so
        // we will copy that behaviour.
        // Select all the objects or components within the marquee.
        MGlobal::selectFromScreen( initialX, initialY, finalX, finalY,
                                  listAdjustment,
                                  MGlobal::kWireframeSelectMethod );
    }
    
    // Get the list of selected items
    MGlobal::getActiveSelectionList(marqueeList);
    
    //we need to set back the previous selection here or the undo for the tcMotionPathCmd won't work
    MGlobal::setActiveSelectionList(incomingList);
    
    //building args and running cmd
    MString cmd("tcMotionPathCmd -selectionChanged ");
    for (unsigned int i = 0; i < marqueeList.length(); ++i)
    {
        MDagPath d;
        marqueeList.getDagPath(i, d);
        if (d.isValid())
            cmd += d.fullPathName() + " ";
    }
    MGlobal::executeCommand(cmd, true, true);
}

void contextUtils::refreshSelectionMethod(MEvent &event, MGlobal::ListAdjustment &listAdjustment)
{
    if (event.isModifierShift() || event.isModifierControl() ) {
        if ( event.isModifierShift() ) {
            if ( event.isModifierControl() ) {
                // both shift and control pressed, merge new selections
                listAdjustment = MGlobal::kAddToList;
            }
            else
            {
                // shift only, xor new selections with previous ones
                listAdjustment = MGlobal::kXORWithList;
            }
        }
        else if ( event.isModifierControl() ) {
            // control only, remove new selections from the previous list
            listAdjustment = MGlobal::kRemoveFromList;
        }
    }
    else
        listAdjustment = MGlobal::kReplaceList;
}


