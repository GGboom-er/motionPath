#include "MotionPathEditContext.h"
#include "GlobalSettings.h"
#include "ContextUtils.h"

extern MotionPathManager mpManager;

// ========== VP2 Implementation (MFrameContext-based, correct viewport handling) ==========

bool MotionPathEditContext::doPressVP2(MEvent &event, const MHWRender::MFrameContext &frameContext)
{
    capsLockCached = isCapsLockOn();
    capsLockValid = true;

    if (capsLockCached) {
        return handleDrawModePress(event, false);
    }

    selectedMotionPathPtr = NULL;
    startedRecording = false;

    event.getPosition(initialX, initialY);

    if (!GlobalSettings::showKeyFrames)
        return false;

    // Get active view for cache lookup (still needed for CameraCache)
    activeView = M3dView::active3dView();
    CameraCache * cachePtr = mpManager.getCameraCachePtrFromView(activeView);

    int selectedCurveId = contextUtils::processCurveHits(initialX, initialY, GlobalSettings::cameraMatrix, activeView, cachePtr, mpManager);

    if (selectedCurveId != -1)
    {
        selectedMotionPathPtr = mpManager.getMotionPathPtr(selectedCurveId);
        if (selectedMotionPathPtr)
        {
            MDagPath camera;
            activeView.getCamera(camera);
            MMatrix cameraMatrix = camera.inclusiveMatrix();
            cameraPosition.x = cameraMatrix(3, 0);
            cameraPosition.y = cameraMatrix(3, 1);
            cameraPosition.z = cameraMatrix(3, 2);

            inverseCameraMatrix = cameraMatrix.inverse();
            selectedMotionPathPtr->setSelectedFromTool(true);

            MIntArray selectedKeys;
            contextUtils::processKeyFrameHits(initialX, initialY, selectedMotionPathPtr, activeView, GlobalSettings::cameraMatrix, cachePtr, selectedKeys);

            if (selectedKeys.length() == 0)
            {
                int selectedTangent = -1;
                if (GlobalSettings::showTangents)
                {
                    int selectedKeyId;
                    contextUtils::processTangentHits(initialX, initialY, selectedMotionPathPtr, activeView, GlobalSettings::cameraMatrix, cachePtr, selectedKeyId, selectedTangent);

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

                if (selectedKeys.length() > 0)
                {
                    lastSelectedTime = selectedMotionPathPtr->getTimeFromKeyId(selectedKeys[0]);
                }

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
        fsDrawn = false;
    }

    return true;
}

void MotionPathEditContext::doDragVP2(MEvent &event, const MHWRender::MFrameContext &frameContext)
{
    if (capsLockValid && capsLockCached) {
        handleDrawModeDrag(event, false);
        return;
    }

    if (!startedRecording && (currentMode == kFrameEditMode || currentMode == kTangentEditMode || currentMode == kShiftKeyMode))
    {
        mpManager.startAnimUndoRecording();
        startedRecording = true;
    }

    short int thisX, thisY;
    event.getPosition(thisX, thisY);

    if (currentMode == kFrameEditMode)
    {
        // VP2: Use MFrameContext for coordinate conversion
        MVector newPosition = contextUtils::getWorldPositionFromProjPointVP2(keyWorldPosition, initialX, initialY, thisX, thisY, frameContext, cameraPosition);

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
        CameraCache * cachePtr = mpManager.getCameraCachePtrFromView(activeView);
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
                {
                    MVector finalOffset = offset;
                    if (GlobalSettings::motionPathDrawMode != GlobalSettings::kWorldSpace) {
                        finalOffset = offset * cachePtr->matrixCache[CameraCache::timeToTick(selectedTimes[j])].inverse();
                    }
                    motionPath->offsetWorldPosition(finalOffset, selectedTimes[j], mpManager.getAnimCurveChangePtr());
                }
                // FIX: 精确范围失效 - 只清除编辑关键帧影响的曲线段
                // 对于每个编辑的关键帧，只需更新[前一个关键帧, 下一个关键帧]范围
                motionPath->invalidateAffectedRanges(selectedTimes);
                // 清空屏幕坐标缓存
                motionPath->invalidateScreenCacheOnly();
            }
        }

        lastWorldPosition = newPosition;
        mpManager.refreshDisplayTimeRange();
    }
    else if (currentMode == kTangentEditMode)
    {
        // VP2: Use MFrameContext for coordinate conversion
        MVector newPosition = contextUtils::getWorldPositionFromProjPointVP2(tangentWorldPosition, initialX, initialY, thisX, thisY, frameContext, cameraPosition);

        // 统一使用世界坐标系更新切线，与doDragCommon保持一致
        MMatrix toWorldMatrix;
        toWorldMatrix.setToIdentity();

        selectedMotionPathPtr->setTangentWorldPosition(newPosition, lastSelectedTime, (Keyframe::Tangent)selectedTangentId, toWorldMatrix, mpManager.getAnimCurveChangePtr());
        // FIX: 精确范围失效 - tangent编辑只影响相邻曲线段
        double outStart, outEnd;
        selectedMotionPathPtr->invalidateAffectedRange(lastSelectedTime, outStart, outEnd);
        selectedMotionPathPtr->invalidateScreenCacheOnly();
        mpManager.refreshDisplayTimeRange();
    }

    // FIX: 请求VP2重新绘制以实时显示拖动更新
    if (currentMode == kFrameEditMode || currentMode == kTangentEditMode)
    {
        if (selectedMotionPathPtr)
        {
            MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
            if (renderer)
            {
                renderer->setGeometryDrawDirty(selectedMotionPathPtr->object());
            }
        }
        // 请求视口刷新 - 使用非阻塞方式
        activeView.refresh(false, true);
    }
}

void MotionPathEditContext::doReleaseVP2(MEvent &event, const MHWRender::MFrameContext &frameContext)
{
    if (capsLockValid && capsLockCached) {
        handleDrawModeRelease(event, false);
        capsLockValid = false;
        return;
    }

    if (!selectedMotionPathPtr)
    {
        short finalX_local, finalY_local;
        event.getPosition(finalX_local, finalY_local);
        contextUtils::applySelection(initialX, initialY, finalX_local, finalY_local, listAdjustment);
    }
    else
    {
        if (startedRecording)
        {
            mpManager.stopDGAndAnimUndoRecording();
            startedRecording = false;
        }

        selectedMotionPathPtr->setSelectedFromTool(false);
        selectedMotionPathPtr = NULL;

        currentMode = kNoneMode;
        alongPreferredAxis = false;
        prefEditAxis = -1;

        // VP2: Refresh viewport after editing keyframes to show updated motion path
        // Required because VP2 doesn't auto-detect animation curve data changes
        activeView.refresh(true, true);
    }

    capsLockValid = false;
}
