
#include "MotionPathEditContext.h"
#include "GlobalSettings.h"
#include "ContextUtils.h"

extern MotionPathManager mpManager;

// Static variable initialization for circle rendering optimization
std::vector<MPoint> MotionPathEditContext::circleVertices;
bool MotionPathEditContext::circleVerticesInitialized = false;

MotionPathEditContextCmd::MotionPathEditContextCmd()
{}

MPxContext* MotionPathEditContextCmd::makeObj()
{
	return new MotionPathEditContext();
}

void* MotionPathEditContextCmd::creator()
{
	return new MotionPathEditContextCmd;
}

MotionPathEditContext::MotionPathEditContext():
	MPxContext(),
	ctxMenuWidget(nullptr)
{
	this->selectedMotionPathPtr = NULL;
    this->currentMode = kNoneMode;

    // Initialize draw mode state
    this->drawMode = kDrawNone;
    this->drawSelectedKeyId = -1;

    // Initialize Caps Lock cache
    this->capsLockCached = false;
    this->capsLockValid = false;

    setTitleString ("MotionPath Edit");
}

MotionPathEditContext::~MotionPathEditContext()
{
	if (ctxMenuWidget)
	{
		delete ctxMenuWidget;
		ctxMenuWidget = nullptr;
	}
}

void MotionPathEditContext::toolOffCleanup()
{
	M3dView view = M3dView::active3dView();
	view.refresh(true, true);

	if (ctxMenuWidget)
	{
		delete ctxMenuWidget;
		ctxMenuWidget = nullptr;
	}

	// Clean up Edit mode state
	if (selectedMotionPathPtr)
	{
		selectedMotionPathPtr->setIsDrawing(false);
		selectedMotionPathPtr->setSelectedFromTool(false);
		selectedMotionPathPtr = nullptr;
	}

	// Reset mode state
	currentMode = kNoneMode;
	drawMode = kDrawNone;
	drawStrokePoints.clear();
	startedRecording = false;

	// Reset Caps Lock cache
	capsLockValid = false;

	// Restore OpenGL default state (Optimization 2)
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);

    for (int i = 0; i < mpManager.getMotionPathsCount(); ++i)
    {
        MotionPath *motionPath = mpManager.getMotionPathPtr(i);
        if (motionPath)
		{
            motionPath->deselectAllKeys();
			motionPath->setIsDrawing(false);
			motionPath->setSelectedFromTool(false);
		}
    }

    M3dView::active3dView().refresh(true, true);

    MPxContext::toolOffCleanup();
}

void MotionPathEditContext::toolOnSetup( MEvent& event )
{
    this->selectedMotionPathPtr = NULL;
    this->currentMode = kNoneMode;
    this->alongPreferredAxis = false;
    this->prefEditAxis = -1;
    this->startedRecording = false;

	// Initialize Draw mode state
	this->drawMode = kDrawNone;
	this->drawSelectedKeyId = -1;
	this->drawStrokePoints.clear();

	// Initialize Caps Lock cache
	this->capsLockValid = false;

	// Setup OpenGL state for drawing (Optimization 2)
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	// set the help text in the maya help boxs
	setHelpString("Left-Click: Select/Move; Shift+Left-Click: Add to selection; CTRL+Left-Click: Toggle selection; CTRL+Left-Click-Drag: Move Selection on the XY plane; CTRL+Middle-Click-Drag: Move Along Y Axis; Right-Click on path/frame/key: show menu");

    M3dView view = M3dView::active3dView();
	view.refresh(true, true);

	if (!ctxMenuWidget)
		ctxMenuWidget = new ContextMenuWidget(view.widget());

	MPxContext::toolOnSetup(event);
	if (view.widget())
		view.widget()->setFocus();
}

void MotionPathEditContext::modifySelection(const MDoubleArray &selectedTimes, const bool ctrl, const bool shift)
{
    if (!selectedMotionPathPtr)
        return;

    mpManager.storePreviousKeySelection();
    for (int i = 0; i < static_cast<int>(selectedTimes.length()); ++i)
    {
        bool thisShift = i != 0 || shift;
        
        if (ctrl)
        {
            if (selectedMotionPathPtr->isKeyAtTimeSelected(selectedTimes[i]))
                selectedMotionPathPtr->deselectKeyAtTime(selectedTimes[i]);
            else
                selectedMotionPathPtr->selectKeyAtTime(selectedTimes[i]);
        }
        else if (thisShift)
        {
            selectedMotionPathPtr->selectKeyAtTime(selectedTimes[i]);
        }
        else
        {
            if (selectedMotionPathPtr->isKeyAtTimeSelected(selectedTimes[i]))
                return;
            
            for (int j=0; j < mpManager.getMotionPathsCount(); ++j)
                mpManager.getMotionPathPtr(j)->deselectAllKeys();
            selectedMotionPathPtr->selectKeyAtTime(selectedTimes[i]);
        }
    }
    MGlobal::executeCommand("tcMotionPathCmd -keySelectionChanged", true, true);
}


bool MotionPathEditContext::doPressCommon(MEvent &event, const bool old)
{
    // === Caps Lock detection: switch between Edit and Draw mode ===
    // Cache Caps Lock state for the entire drag operation (Optimization 1)
    capsLockCached = isCapsLockOn();
    capsLockValid = true;

    if (capsLockCached) {
        // Caps ON → Draw mode (batch operations)
        return handleDrawModePress(event, old);
    }

    // Caps OFF → Edit mode (single-point operations)
    selectedMotionPathPtr = NULL;
    startedRecording = false;

    event.getPosition(initialX, initialY);
    activeView = M3dView::active3dView();
    
    if (!GlobalSettings::showKeyFrames)
        return false;
    
    CameraCache * cachePtr = mpManager.MotionPathManager::getCameraCachePtrFromView(activeView);
    
	int selectedCurveId;
	if (!old)
		selectedCurveId = contextUtils::processCurveHits(initialX, initialY, GlobalSettings::cameraMatrix, activeView, cachePtr, mpManager);
	else
		selectedCurveId = contextUtils::processCurveHits(activeView, cachePtr, mpManager);

    if (selectedCurveId != -1)
    {
        selectedMotionPathPtr = mpManager.getMotionPathPtr(selectedCurveId);
        if (selectedMotionPathPtr)
        {
            MDagPath camera;
            activeView.getCamera(camera);
			MMatrix cameraMatrix = camera.inclusiveMatrix();
            cameraPosition.x = cameraMatrix(3, 0);
            cameraPosition.y = cameraMatrix(3, 1),
            cameraPosition.z = cameraMatrix(3, 2);
            
            inverseCameraMatrix = cameraMatrix.inverse();
            
            selectedMotionPathPtr->setSelectedFromTool(true);
            
            MIntArray selectedKeys;

			if (!old)
				contextUtils::processKeyFrameHits(initialX, initialY, selectedMotionPathPtr, activeView, GlobalSettings::cameraMatrix, cachePtr, selectedKeys);
			else
				contextUtils::processKeyFrameHits(selectedMotionPathPtr, activeView, cachePtr, selectedKeys);

            if (selectedKeys.length() == 0)
            {
                int selectedTangent = -1;
                if (GlobalSettings::showTangents)
                {
                    int selectedKeyId;

					if (!old)
						contextUtils::processTangentHits(initialX, initialY, selectedMotionPathPtr, activeView, GlobalSettings::cameraMatrix, cachePtr, selectedKeyId, selectedTangent);
					else
						contextUtils::processTangentHits(selectedMotionPathPtr, activeView, cachePtr, selectedKeyId, selectedTangent);

                    //move tangent
                    if (selectedTangent != -1)
                    {                        
                        currentMode = kTangentEditMode;
                        selectedTangentId = selectedTangent;
                        tangentWorldPosition = MVector::zero;
                        
                        lastSelectedTime = selectedMotionPathPtr->getTimeFromKeyId(selectedKeyId);
                        
                        selectedMotionPathPtr->getTangentHandleWorldPosition(lastSelectedTime, (Keyframe::Tangent)selectedTangentId, tangentWorldPosition);
                        lastWorldPosition = tangentWorldPosition;
                        
                        selectedMotionPathPtr->getKeyWorldPosition(lastSelectedTime, keyWorldPosition);
                    }
                }
            }
            else
            {
                if(event.mouseButton() == MEvent::kMiddleMouse)
                {
                    //move along the major axis
                    alongPreferredAxis = true;
                    currentMode = kFrameEditMode;
                    prefEditAxis = event.isModifierControl() ? 1: 0;
                }
                else
                    currentMode = kFrameEditMode;
                
                keyWorldPosition = MVector::zero;

                MDoubleArray times;
                for (int i=0; i < static_cast<int>(selectedKeys.length()); ++i)
                {
                    lastSelectedTime = selectedMotionPathPtr->getTimeFromKeyId(selectedKeys[i]);
                    times.append(lastSelectedTime);
                }

                // Fix: Use the first selected key as reference for dragging (the one user clicked)
                // This ensures drag follows mouse correctly
                if (selectedKeys.length() > 0)
                {
                    lastSelectedTime = selectedMotionPathPtr->getTimeFromKeyId(selectedKeys[0]);
                }

                // Only allow left mouse button to modify selection, middle button only moves
                // Fix: Only left button (without Ctrl) should modify selection
                if (event.mouseButton() == MEvent::kLeftMouse && !event.isModifierControl())
                {
                    modifySelection(times, false, event.isModifierShift());
                }

                selectedMotionPathPtr->getKeyWorldPosition(lastSelectedTime, keyWorldPosition);
                lastWorldPosition = keyWorldPosition;
            }
        }
    }
    else
    {
        contextUtils::refreshSelectionMethod(event, listAdjustment);
        
        if (old)
            fsDrawn = false;
    }
    
    return true;
}

MStatus MotionPathEditContext::doPress(MEvent &event)
{
    return doPressCommon(event, true)? MStatus::kSuccess: MStatus::kFailure;
}

MStatus MotionPathEditContext::doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
    return doPressCommon(event, false) ? MStatus::kSuccess: MStatus::kFailure;
}

void MotionPathEditContext::doDragCommon(MEvent &event, const bool old)
{
    // === Caps Lock detection ===
    // Use cached Caps Lock state (Optimization 1)
    if (capsLockValid && capsLockCached) {
        handleDrawModeDrag(event, old);
        return;
    }

    // Edit mode
    if (!startedRecording && (currentMode == kFrameEditMode || currentMode == kTangentEditMode || currentMode == kShiftKeyMode))
    {
        mpManager.startAnimUndoRecording();
        startedRecording = true;
    }
    
    short int thisX, thisY;
    event.getPosition(thisX, thisY);
    M3dView view = M3dView::active3dView();
    
    if (currentMode == kFrameEditMode)
    {
        
        MVector newPosition = contextUtils::getWorldPositionFromProjPoint(keyWorldPosition, initialX, initialY, thisX, thisY, view, cameraPosition);
         
        //if control is pressed with find the axis with the maximum value and we move the selected keyframes only on that axis
        if (alongPreferredAxis)
        {
            if (prefEditAxis == 0)
                newPosition[1] = keyWorldPosition[1];
            else
            {
                newPosition[0] = keyWorldPosition[0];
                newPosition[2] = keyWorldPosition[2];
            }
        }
        
        MVector offset = newPosition - lastWorldPosition;
        CameraCache * cachePtr = mpManager.MotionPathManager::getCameraCachePtrFromView(activeView);
        if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
        {
            if (!cachePtr)
                return;
            offset = offset * inverseCameraMatrix;
        }
    
        for (int i = 0; i < mpManager.getMotionPathsCount(); i++)
        {
            MotionPath *motionPath = mpManager.getMotionPathPtr(i);
            if (motionPath)
            {
                MDoubleArray selectedTimes = motionPath->getSelectedKeys();
                for (int j = 0; j < static_cast<int>(selectedTimes.length()); j++)
                    motionPath->offsetWorldPosition(GlobalSettings::motionPathDrawMode == GlobalSettings::kWorldSpace ? offset : offset * cachePtr->matrixCache[selectedTimes[j]].inverse(), selectedTimes[j], mpManager.getAnimCurveChangePtr());
            }
        }
        
        lastWorldPosition = newPosition;
    }
    else if (currentMode == kTangentEditMode)
    {
        MVector newPosition = contextUtils::getWorldPositionFromProjPoint(tangentWorldPosition, initialX, initialY, thisX, thisY, view, cameraPosition);
        
        MMatrix toWorldMatrix;
        if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
        {
            CameraCache * cachePtr = mpManager.MotionPathManager::getCameraCachePtrFromView(activeView);
            if (!cachePtr)
                return;
            //newPosition = MVector(MPoint(newPosition) * inverseCameraMatrix * cachePtr->matrixCache[lastSelectedTime].inverse());
            toWorldMatrix = inverseCameraMatrix * cachePtr->matrixCache[lastSelectedTime].inverse();
        }
        else
            toWorldMatrix.setToIdentity();
        
        selectedMotionPathPtr->setTangentWorldPosition(newPosition, lastSelectedTime, (Keyframe::Tangent)selectedTangentId, toWorldMatrix, mpManager.getAnimCurveChangePtr());

    }

    // 保留刷新以提供实时视觉反馈（功能优先于优化）
    view.refresh(true, true);
}

MStatus MotionPathEditContext::doDrag(MEvent &event)
{
    if (selectedMotionPathPtr)
    {
        doDragCommon(event, true);

        // Draw preview for Draw modes (Caps Lock ON)
        // 使用缓存而非系统调用（优化13）
        if (capsLockValid && capsLockCached)
        {
            if (drawMode == kDraw)
            {
                // Draw preview path and keyframes for kDraw mode (Legacy OpenGL)
                activeView.beginXorDrawing(true, true, 2.0f, M3dView::kStippleNone);
                drawPreviewPath();
                activeView.endXorDrawing();
            }
            else if (drawMode == kStroke)
            {
                // Draw white stroke line for kStroke mode (Legacy OpenGL)
                activeView.beginXorDrawing(true, true, 2.0f, M3dView::kStippleNone);

                // Draw stroke with white line
                if (drawStrokePoints.length() >= 2)
                {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glEnable(GL_LINE_SMOOTH);
                    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

                    // Draw thick outer stroke for better visibility
                    glLineWidth(4.0f);
                    glColor4f(0.2f, 0.2f, 0.2f, 0.6f); // Dark semi-transparent outline
                    glBegin(GL_LINE_STRIP);
                    for (unsigned int i = 0; i < drawStrokePoints.length(); ++i)
                        glVertex2f(static_cast<float>(drawStrokePoints[i].x), static_cast<float>(drawStrokePoints[i].y));
                    glEnd();

                    // Draw inner bright stroke
                    glLineWidth(2.0f);
                    glColor4f(1.0f, 1.0f, 1.0f, 0.95f); // Bright white
                    glBegin(GL_LINE_STRIP);
                    for (unsigned int i = 0; i < drawStrokePoints.length(); ++i)
                        glVertex2f(static_cast<float>(drawStrokePoints[i].x), static_cast<float>(drawStrokePoints[i].y));
                    glEnd();

                    glDisable(GL_LINE_SMOOTH);
                }

                activeView.endXorDrawing();
            }
        }
    }
    else
    {
        activeView.beginXorDrawing();
        // Redraw the marquee at its old position to erase it.
        if (fsDrawn)
            contextUtils::drawMarqueeGL(initialX, initialY, finalX, finalY);

        fsDrawn = true;

        event.getPosition( finalX, finalY );
        // Draw the marquee at its new position.
        contextUtils::drawMarqueeGL(initialX, initialY, finalX, finalY);

        activeView.endXorDrawing();
    }

    return MStatus::kSuccess;
}

MStatus MotionPathEditContext::doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
    if (selectedMotionPathPtr)
    {
        doDragCommon(event, false);

        // Draw preview for Draw modes (Caps Lock ON)
        if (isCapsLockOn())
        {
            if (drawMode == kStroke)
            {
                // Draw white stroke line for kStroke mode (VP2)
                if (drawStrokePoints.length() >= 2)
                {
                    drawMgr.beginDrawable();

                    // Draw dark outline for better visibility
                    drawMgr.setLineWidth(4.0f);
                    drawMgr.setColor(MColor(0.2f, 0.2f, 0.2f, 0.6f));
                    drawMgr.setLineStyle(MHWRender::MUIDrawManager::kSolid);
                    for (unsigned int i = 1; i < drawStrokePoints.length(); ++i)
                        drawMgr.line2d(drawStrokePoints[i-1], drawStrokePoints[i]);

                    // Draw bright center line
                    drawMgr.setLineWidth(2.0f);
                    drawMgr.setColor(MColor(1.0f, 1.0f, 1.0f, 0.95f));
                    for (unsigned int i = 1; i < drawStrokePoints.length(); ++i)
                        drawMgr.line2d(drawStrokePoints[i-1], drawStrokePoints[i]);

                    drawMgr.endDrawable();
                }
            }
            else if (drawMode == kDraw)
            {
                // Draw preview path and keyframes for kDraw mode (VP2)
                if (drawStrokePoints.length() > 1)
                {
                    drawMgr.beginDrawable();
                    drawMgr.setColor(GlobalSettings::previewPathColor);
                    drawMgr.setLineWidth(3.0f);
                    drawMgr.setLineStyle(MHWRender::MUIDrawManager::kDashed);

                    for (unsigned int i = 1; i < drawStrokePoints.length(); ++i)
                    {
                        drawMgr.line2d(MPoint(drawStrokePoints[i-1].x, drawStrokePoints[i-1].y),
                                       MPoint(drawStrokePoints[i].x, drawStrokePoints[i].y));
                    }

                    drawMgr.setLineStyle(MHWRender::MUIDrawManager::kSolid);

                    // Draw preview keyframe markers (skip start point)
                    int keyframeCount = GlobalSettings::drawKeyframeCount;
                    int totalPoints = drawStrokePoints.length();

                    for (int i = 0; i < keyframeCount; ++i)
                    {
                        // Calculate point index (skip first point)
                        int pointIndex = (int)((double)(i + 1) * (totalPoints - 1) / (keyframeCount + 1));
                        if (pointIndex >= totalPoints) pointIndex = totalPoints - 1;
                        if (pointIndex < 1) pointIndex = 1;

                        MVector screenPos = drawStrokePoints[pointIndex];

                        drawMgr.setColor(GlobalSettings::previewKeyframeColor);
                        drawMgr.circle2d(MPoint(screenPos.x, screenPos.y), 8.0, true);
                        drawMgr.setColor(MColor(0.2, 0.2, 0.2));
                        drawMgr.circle2d(MPoint(screenPos.x, screenPos.y), 9.0, false);
                    }

                    drawMgr.endDrawable();
                }
            }
        }
    }
    else
    {
        //  Get the marquee's new end position.
        event.getPosition( finalX, finalY );
        // Draw the marquee at its new position.
        contextUtils::drawMarquee(drawMgr, initialX, initialY, finalX, finalY);
    }
    return MS::kSuccess;
}

void MotionPathEditContext::doReleaseCommon(MEvent &event, const bool old)
{
    // === Caps Lock detection ===
    // Use cached Caps Lock state and invalidate cache (Optimization 1)
    if (capsLockCached) {
        handleDrawModeRelease(event, old);
        capsLockValid = false;  // Invalidate cache after release
        return;
    }

    // Invalidate cache for Edit mode as well
    capsLockValid = false;

    // Edit mode
    if (selectedMotionPathPtr)
    {
        if(startedRecording && (currentMode == kFrameEditMode || currentMode == kTangentEditMode || currentMode == kShiftKeyMode))
            mpManager.stopDGAndAnimUndoRecording();
        
        selectedMotionPathPtr->setSelectedFromTool(false);
        selectedMotionPathPtr = NULL;
        currentMode = kNoneMode;
        
        alongPreferredAxis = false;
        prefEditAxis = -1;
        
        M3dView view = M3dView::active3dView();
		view.refresh(true, true);
    }
    else
    {
        event.getPosition( finalX, finalY );
        
        if (fsDrawn && old)
        {
            activeView.beginXorDrawing();
            contextUtils::drawMarqueeGL(initialX, initialY, finalX, finalY);
            activeView.endXorDrawing();
        }
        
        contextUtils::applySelection(initialX, initialY, finalX, finalY, listAdjustment);
    }
}

MStatus MotionPathEditContext::doRelease(MEvent &event)
{
    doReleaseCommon(event, true);
    return MStatus::kSuccess;
}

MStatus MotionPathEditContext::doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
    doReleaseCommon(event, false);
    return MS::kSuccess;
}

//====================================================================================
// CAPS LOCK DETECTION AND DRAW MODE INTEGRATION
//====================================================================================

bool MotionPathEditContext::isCapsLockOn() const
{
    #ifdef _WIN32
        // Windows: GetKeyState returns toggle state in low-order bit
        SHORT keyState = GetKeyState(VK_CAPITAL);
        return (keyState & 0x0001) != 0;
    #elif defined(__APPLE__)
        // macOS: CGEventSourceFlagsState
        CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
        return (flags & kCGEventFlagMaskAlphaShift) != 0;
    #else
        // Linux/other platforms: fallback to Edit mode
        return false;
    #endif
}


// Temporary file for Draw mode implementation
// This needs to be appended to MotionPathEditContext.cpp

bool MotionPathEditContext::handleDrawModePress(MEvent &event, const bool old)
{
    event.getPosition(initialX, initialY);
    activeView = M3dView::active3dView();

    if (!GlobalSettings::showKeyFrames)
        return false;

    MDagPath camera;
    activeView.getCamera(camera);
    MMatrix cameraMatrix = camera.inclusiveMatrix();
    cameraPosition.x = cameraMatrix(3, 0);
    cameraPosition.y = cameraMatrix(3, 1),
    cameraPosition.z = cameraMatrix(3, 2);

    inverseCameraMatrix = cameraMatrix.inverse();

    // Middle mouse button removed - use Edit mode right-click menu

    CameraCache * cachePtr = mpManager.MotionPathManager::getCameraCachePtrFromView(activeView);
    int selectedCurveId;
    if (!old)
        selectedCurveId = contextUtils::processCurveHits(initialX, initialY, GlobalSettings::cameraMatrix, activeView, cachePtr, mpManager);
    else
        selectedCurveId = contextUtils::processCurveHits(activeView, cachePtr, mpManager);

    if (selectedCurveId != -1)
    {
        selectedMotionPathPtr = mpManager.getMotionPathPtr(selectedCurveId);
        if (selectedMotionPathPtr)
        {
            selectedMotionPathPtr->setSelectedFromTool(true);

            MIntArray ids;
            if (!old)
                contextUtils::processKeyFrameHits(initialX, initialY, selectedMotionPathPtr, activeView, GlobalSettings::cameraMatrix, cachePtr, ids);
            else
                contextUtils::processKeyFrameHits(selectedMotionPathPtr, activeView, cachePtr, ids);
            if (ids.length() > 0)
            {
                drawSelectedKeyId = ids[ids.length() - 1];

                drawKeyWorldPosition = MVector::zero;
                drawSelectedTime = selectedMotionPathPtr->getTimeFromKeyId(drawSelectedKeyId);
                selectedMotionPathPtr->getKeyWorldPosition(drawSelectedTime, drawKeyWorldPosition);
                selectedMotionPathPtr->selectKeyAtTime(drawSelectedTime);

                mpManager.startAnimUndoRecording();

                if (event.isModifierControl())
                {
                    drawMode = kStroke;

                    // Pre-allocate vector memory to avoid reallocations during drag (Optimization 4)
                    if (drawStrokePoints.length() == 0 || drawStrokePoints.length() < 1000)
                        drawStrokePoints.setLength(1000);
                    drawStrokePoints.clear();
                    drawStrokePoints.append(MVector(initialX, initialY, 0));
                }
                else
                {
                    drawMode = kDraw;
                    // 移除热路径日志以提升性能（优化11）

                    selectedMotionPathPtr->setIsDrawing(true);
                    selectedMotionPathPtr->setEndrawingTime(drawSelectedTime);

                    drawMaxTime = MAnimControl::maxTime().as(MTime::uiUnit());
                    drawSteppedTime = drawSelectedTime;

                    // Pre-allocate vector memory to avoid reallocations during drag (Optimization 4)
                    if (drawStrokePoints.length() == 0 || drawStrokePoints.length() < 1000)
                        drawStrokePoints.setLength(1000);
                    drawStrokePoints.clear();
                    drawStrokePoints.append(MVector(initialX, initialY, 0));

                    // 移除热路径日志以提升性能（优化11）
                    // 用户可通过UI状态栏查看这些信息

                    drawInitialClock = clock();
                }

                activeView.refresh();

                return true;
            }
        }
    }
    else
    {
        contextUtils::refreshSelectionMethod(event, listAdjustment);

        if (old)
            fsDrawn = false;
    }

    return false;
}

bool MotionPathEditContext::handleDrawModeDrag(MEvent &event, const bool old)
{
    if (selectedMotionPathPtr)
    {
        // 优化12：合并 kDraw 和 kStroke 的重复代码
        // 两种模式使用相同的路径采样逻辑
        if (drawMode == kDraw || drawMode == kStroke)
        {
            short int thisX, thisY;
            event.getPosition(thisX, thisY);
            MVector v(thisX, thisY, 0);

            if (drawStrokePoints.length() > 0)
            {
                double distance = (v - drawStrokePoints[drawStrokePoints.length()-1]).length();
                double threshold = 8.0;
                if (distance > threshold)
                    drawStrokePoints.append(v);
            }
            else
            {
                drawStrokePoints.append(v);
            }
        }
    }
    return true;
}

bool MotionPathEditContext::handleDrawModeRelease(MEvent &event, const bool old)
{
    if (selectedMotionPathPtr)
    {
        if (drawMode == kStroke)
        {
            // kStroke mode: adjust existing keyframes along stroke path
            int strokeNum = drawStrokePoints.length() - 1;
            if (strokeNum > 1)
            {
                // Get the direction vector
                MVector directionalVector;
                for (unsigned int i = 1; i < drawStrokePoints.length(); ++i)
                {
                    directionalVector += drawStrokePoints[i] - drawStrokePoints[0];
                }
                directionalVector /= strokeNum;
                directionalVector.normalize();

                // Get keyframes
                MDoubleArray keys = selectedMotionPathPtr->getKeys();

                int selectedIndex = 0;
                for (; selectedIndex < static_cast<int>(keys.length()); ++selectedIndex)
                    if (keys[selectedIndex] == drawSelectedTime)
                        break;

                int direction = getStrokeDirection(directionalVector, keys, selectedIndex);
                if (direction != 0)
                {
                    // Collect keyframes in stroke direction
                    struct StrokeCache {
                        MVector originalScreenPosition;
                        MVector screenPosition;
                        MVector originalWorldPosition;
                        double time;
                    };

                    std::vector<StrokeCache> cache, tempCache;

                    // 修复：增加跳过限制，避免关键帧多时失效
                    int MAX_SKIPPED = 50, skipped = 0;  // 从5增加到50

                    MVector lastStrokePos = drawStrokePoints[strokeNum];
                    double distance = (lastStrokePos - getkeyScreenPosition(keys[selectedIndex])).length();
                    for (int i = selectedIndex + direction;
                         i >= 0 && i < static_cast<int>(keys.length());
                         i += direction)
                    {
                        MVector pos = getkeyScreenPosition(keys[i]);
                        double thisDistance = (lastStrokePos - pos).length();

                        if (thisDistance > distance)
                        {
                            skipped++;
                            if (skipped > MAX_SKIPPED)
                                break;

                            if (i == 0 || i == keys.length() - 1)
                                break;

                            StrokeCache c;
                            c.originalScreenPosition = pos;
                            c.time = keys[i];
                            selectedMotionPathPtr->getKeyWorldPosition(keys[i], pos);
                            c.originalWorldPosition = pos;
                            tempCache.push_back(c);
                            continue;
                        }

                        skipped = 0;
                        if (tempCache.size() > 0)
                        {
                            cache.insert(cache.end(), tempCache.begin(), tempCache.end());
                            tempCache.clear();
                        }

                        distance = thisDistance;

                        StrokeCache c;
                        c.originalScreenPosition = pos;
                        c.time = keys[i];
                        selectedMotionPathPtr->getKeyWorldPosition(keys[i], pos);
                        c.originalWorldPosition = pos;
                        cache.push_back(c);

                        if (i == 0 || i == keys.length() - 1)
                            break;
                    }

                    int pointSize = static_cast<int>(cache.size());
                    if (pointSize > 0)
                    {
                        // Delete keyframes to recalculate tangents
                        for (int i = static_cast<int>(cache.size()) - 1; i > -1 ; --i)
                            selectedMotionPathPtr->deleteKeyFrameAtTime(cache[i].time, mpManager.getAnimCurveChangePtr(), false);

                        // Get stroke length
                        double strokeLenght = 0;
                        std::vector<double> segmentLenghts;
                        segmentLenghts.resize(strokeNum);
                        for (int i = 1; i < strokeNum; ++i)
                        {
                            segmentLenghts[i-1] = (drawStrokePoints[i] - drawStrokePoints[i - 1]).length();
                            strokeLenght += segmentLenghts[i-1];
                        }

                        // Match each key using the right mode (closest or spread)
                        for (int i = 0; i < pointSize ; ++i)
                        {
                            if (GlobalSettings::strokeMode == 0) // closest
                                cache[i].screenPosition = getClosestPointOnPolyLine(cache[i].originalScreenPosition);
                            else // spread
                                cache[i].screenPosition = getSpreadPointOnPolyLine(i, pointSize, strokeLenght, segmentLenghts);

                            MVector newPosition = contextUtils::getWorldPositionFromProjPoint(
                                cache[i].originalWorldPosition,
                                cache[i].originalScreenPosition.x, cache[i].originalScreenPosition.y,
                                cache[i].screenPosition.x, cache[i].screenPosition.y,
                                activeView, cameraPosition);

                            if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
                            {
                                MPoint worldPos = newPosition;
                                if (!contextUtils::worldCameraSpaceToWorldSpace(worldPos, activeView, cache[i].time, inverseCameraMatrix, mpManager))
                                    continue;
                                newPosition = worldPos;
                            }

                            selectedMotionPathPtr->addKeyFrameAtTime(cache[i].time, mpManager.getAnimCurveChangePtr(), &newPosition, false);
                        }
                    }
                }
            }
        }
        else if (drawMode == kDraw)
        {
            // Batch add keyframes from drawn path
            if (drawStrokePoints.length() > 1)
            {
                int keyframeCount = GlobalSettings::drawKeyframeCount;
                int frameInterval = GlobalSettings::drawFrameInterval;

                // Validate frame interval
                if (frameInterval <= 0)
                {
                    MGlobal::displayWarning("[MotionPath] Invalid frame interval, using default 1");
                    frameInterval = 1;
                }

                // Boundary check
                int totalPoints = drawStrokePoints.length();
                if (totalPoints < 2)
                {
                    MGlobal::displayWarning("[MotionPath] Not enough points to sample keyframes. Draw a longer path.");
                    selectedMotionPathPtr->setIsDrawing(false);
                    selectedMotionPathPtr->deselectAllKeys();
                    selectedMotionPathPtr->setSelectedFromTool(false);
                    selectedMotionPathPtr = NULL;
                    drawMode = kDrawNone;
                    drawStrokePoints.clear();
                    mpManager.stopDGAndAnimUndoRecording();
                    activeView.refresh();
                    return true;
                }

                double rangeEnd = drawSelectedTime + (keyframeCount * frameInterval);

                // 移除热路径日志以提升性能（优化11）
                // 用户可通过UI Toast提示查看操作结果

                // Delete existing keyframes in range
                selectedMotionPathPtr->deleteAllKeyFramesInRange(drawSelectedTime, rangeEnd, mpManager.getAnimCurveChangePtr());

                // Sample and add keyframes
                for (int i = 0; i < keyframeCount; ++i)
                {
                    int pointIndex = (int)((double)(i + 1) * (totalPoints - 1) / (keyframeCount + 1));
                    if (pointIndex >= totalPoints) pointIndex = totalPoints - 1;
                    if (pointIndex < 1) pointIndex = 1;

                    MVector screenPos = drawStrokePoints[pointIndex];
                    double keyTime = drawSelectedTime + ((i + 1) * frameInterval);

                    MVector worldPos = contextUtils::getWorldPositionFromProjPoint(
                        drawKeyWorldPosition,
                        initialX, initialY,
                        screenPos.x, screenPos.y,
                        activeView, cameraPosition);

                    if (GlobalSettings::motionPathDrawMode == GlobalSettings::kCameraSpace)
                    {
                        MPoint worldPosPoint = worldPos;
                        if (!contextUtils::worldCameraSpaceToWorldSpace(worldPosPoint, activeView,
                                                                          keyTime, inverseCameraMatrix, mpManager))
                            continue;
                        worldPos = worldPosPoint;
                    }

                    selectedMotionPathPtr->addKeyFrameAtTime(keyTime, mpManager.getAnimCurveChangePtr(), &worldPos, false);
                }

                // 移除热路径日志以提升性能（优化11）
                // 用户可通过UI Toast提示看到"✓ 已添加 N 个关键帧"
                selectedMotionPathPtr->setEndrawingTime(rangeEnd);
                mpManager.refreshDisplayTimeRange();
            }
        }

        if (drawMode != kDrawNone)
            mpManager.stopDGAndAnimUndoRecording();

        selectedMotionPathPtr->deselectAllKeys();
        selectedMotionPathPtr->setSelectedFromTool(false);
        selectedMotionPathPtr->setIsDrawing(false);
        activeView.refresh();

        selectedMotionPathPtr = NULL;
        drawMode = kDrawNone;
        drawStrokePoints.clear();
    }
    return true;
}

// === Draw mode preview rendering (OpenGL) ===
// Initialize pre-computed circle vertices (Optimization 3)
void MotionPathEditContext::initializeCircleVertices()
{
    if (circleVerticesInitialized)
        return;

    circleVertices.clear();
    circleVertices.reserve(13);  // 360/30 + 1 = 13 vertices

    for (int angle = 0; angle <= 360; angle += 30)
    {
        float rad = angle * 3.14159f / 180.0f;
        circleVertices.push_back(MPoint(cos(rad), sin(rad), 0));
    }

    circleVerticesInitialized = true;
}

void MotionPathEditContext::drawPreviewPath()
{
    if (drawStrokePoints.length() < 2)
        return;

    // Initialize circle vertices once (Optimization 3)
    initializeCircleVertices();

    // Note: OpenGL state already set in toolOnSetup (Optimization 2)
    // No need to enable/disable GL_BLEND and GL_LINE_SMOOTH here

    // Draw preview path with distinct orange color and dashed style
    MColor previewColor = GlobalSettings::previewPathColor;
    glLineWidth(3.0f);
    glColor4f(previewColor.r, previewColor.g, previewColor.b, previewColor.a);

    // Enable line stipple for dashed appearance
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(2, 0x00FF);  // Dashed pattern

    glBegin(GL_LINE_STRIP);
    for (unsigned int i = 0; i < drawStrokePoints.length(); ++i)
        glVertex2f(static_cast<float>(drawStrokePoints[i].x), static_cast<float>(drawStrokePoints[i].y));
    glEnd();

    glDisable(GL_LINE_STIPPLE);

    // Draw preview keyframe markers using pre-computed vertices (Optimization 3)
    int keyframeCount = GlobalSettings::drawKeyframeCount;
    int totalPoints = drawStrokePoints.length();

    MColor keyframeColor = GlobalSettings::previewKeyframeColor;
    float markerSize = 8.0f;

    for (int i = 0; i < keyframeCount; ++i)
    {
        // Calculate point index (skip first point)
        int pointIndex = (int)((double)(i + 1) * (totalPoints - 1) / (keyframeCount + 1));
        if (pointIndex >= totalPoints) pointIndex = totalPoints - 1;
        if (pointIndex < 1) pointIndex = 1;

        MVector screenPos = drawStrokePoints[pointIndex];

        // Draw black background circle for contrast using pre-computed vertices
        glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(screenPos.x, screenPos.y);
        float outerSize = markerSize + 1.0f;
        for (const auto& v : circleVertices)
        {
            glVertex2f(screenPos.x + v.x * outerSize,
                      screenPos.y + v.y * outerSize);
        }
        glEnd();

        // Draw colored keyframe marker using pre-computed vertices
        glColor4f(keyframeColor.r, keyframeColor.g, keyframeColor.b, 1.0f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(screenPos.x, screenPos.y);
        for (const auto& v : circleVertices)
        {
            glVertex2f(screenPos.x + v.x * markerSize,
                      screenPos.y + v.y * markerSize);
        }
        glEnd();
    }

    // Note: OpenGL state restored in toolOffCleanup (Optimization 2)
}

void MotionPathEditContext::drawPreviewKeyframes() {}

// === Helper functions for kStroke mode ===
MVector MotionPathEditContext::getkeyScreenPosition(const double time)
{
    MVector pos;
    selectedMotionPathPtr->getKeyWorldPosition(time, pos);
    short thisX, thisY;
    activeView.worldToView(pos, thisX, thisY);
    return MVector(thisX, thisY, 0);
}

int MotionPathEditContext::getStrokeDirection(MVector directionalVector, const MDoubleArray &keys, const int selectedIndex)
{
    MVector pos = getkeyScreenPosition(keys[selectedIndex]);
    MVector pp = selectedIndex == 0 ? MVector(0,0,0): getkeyScreenPosition(keys[selectedIndex-1]) - pos;
    MVector ap = selectedIndex == keys.length() - 1 ? MVector(0,0,0): getkeyScreenPosition(keys[selectedIndex+1]) - pos;
    pp.normalize();
    ap.normalize();

    double dot1 = pp * directionalVector;
    double dot2 = ap * directionalVector;

    if (dot1 == 0 && dot2 < 0)
        return 0;

    if (dot2 == 0 && dot1 < 0)
        return 0;

    return dot1 > dot2 ? -1: 1;
}

MVector MotionPathEditContext::getClosestPointOnPolyLine(const MVector &q)
{
    const int count = drawStrokePoints.length();
    double finalT = 0;
    int index = 0;

    MVector b = drawStrokePoints[0], dbq = b - q;
    double dist = (dbq.x*dbq.x + dbq.y*dbq.y);
    for (size_t i = 1; i < count; ++i)
    {
        const MVector a = b, daq = dbq;

        b = drawStrokePoints[i];
        dbq = b - q;

        const MVector dab = a - b;
        double sqrlen = dab.x*dab.x + dab.y*dab.y;
        if (sqrlen < 1e-10)  // Points are identical, skip this segment
            continue;
        const double inv_sqrlen = 1.0 / sqrlen;
        const double t = (dab.x*daq.x + dab.y*daq.y) * inv_sqrlen;
        if (t<0.)
            continue;
        double current_dist;
        if (t<=1.)
            current_dist = (dab.x*dbq.y - dab.y*dbq.x) * (dab.x*dbq.y - dab.y*dbq.x) * inv_sqrlen;
        else//t>1.
            current_dist = (dbq.x*dbq.x + dbq.y*dbq.y);

        if (current_dist<dist)
        {
            dist = current_dist;
            finalT = t > 1 ? 1: t;
            index = i;
        }
    }

    if (finalT == 0 && index == 0)
        return drawStrokePoints[0];
    else
        return drawStrokePoints[index] * finalT + drawStrokePoints[index-1] * (1-finalT);
}

MVector MotionPathEditContext::getSpreadPointOnPolyLine(const int i, const int pointSize, const double strokeLenght, const std::vector<double> &segmentLenghts)
{
    if (i == pointSize-1)
    {
        if (drawStrokePoints.length() == 0)
            return MVector::zero;
        return drawStrokePoints[drawStrokePoints.length() - 1];
    }
    else
    {
        //we do +1 as the first key is not evaluated, and with pointSize there no -1
        float tl = (((float)i+1) / ((float)pointSize)) * strokeLenght;

        int currentSegmentIndex = 0;
        double currentSegmentLenght = 0;
        bool found = false;
        for (int j = 0; j < static_cast<int>(drawStrokePoints.length()) - 1; ++j)
        {
            if (tl > currentSegmentLenght && tl < currentSegmentLenght + segmentLenghts[j])
            {
                currentSegmentIndex = j;
                found = true;
                break;
            }
            else
                currentSegmentLenght += segmentLenghts[j];
        }

        // If not found, use the last segment
        if (!found && drawStrokePoints.length() >= 2)
            currentSegmentIndex = static_cast<int>(drawStrokePoints.length()) - 2;

        // Check for zero-length segment
        if (segmentLenghts[currentSegmentIndex] < 1e-6)
            return drawStrokePoints[currentSegmentIndex];

        float t = (tl - currentSegmentLenght) / (segmentLenghts[currentSegmentIndex]);
        return drawStrokePoints[currentSegmentIndex+1] * t + drawStrokePoints[currentSegmentIndex] * (1 - t);
    }
}

double MotionPathEditContext::calculatePathLength(const MVectorArray &points)
{
    double length = 0.0;
    for (unsigned int i = 1; i < points.length(); ++i)
    {
        length += (points[i] - points[i-1]).length();
    }
    return length;
}

MVector MotionPathEditContext::samplePointOnPath(double t, const MVectorArray &points, double totalLength)
{
    if (points.length() == 0)
        return MVector::zero;
    if (t <= 0.0 || points.length() == 1)
        return points[0];
    if (t >= 1.0)
        return points[points.length() - 1];

    double targetLength = t * totalLength;
    double currentLength = 0.0;

    for (unsigned int i = 1; i < points.length(); ++i)
    {
        double segmentLength = (points[i] - points[i-1]).length();

        if (currentLength + segmentLength >= targetLength)
        {
            // Interpolate within this segment
            double segmentT = (targetLength - currentLength) / segmentLength;
            return points[i-1] * (1.0 - segmentT) + points[i] * segmentT;
        }

        currentLength += segmentLength;
    }

    return points[points.length() - 1];
}
