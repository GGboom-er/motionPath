//
//  MotionPathEditContextMenuWidget.cpp
//  MotionPath
//
//  Created by Daniele Federico on 24/01/15.
//
//

#include <QWidget>
#include <QMenu>
#include <QPointer>
#include <QAction>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>

#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <maya/MStringArray.h>
#include <maya/MAnimControl.h>

#include "MotionPathEditContextMenuWidget.h"
#include "ContextUtils.h"
#include "KeyClipboard.h"

extern MotionPathManager mpManager;

ContextMenuWidget::ContextMenuWidget(QWidget *parent): QWidget(parent)
{
	m_parent = parent;
	this->setMouseTracking(true);
	m_parent->installEventFilter(this);
    
}

ContextMenuWidget::~ContextMenuWidget()
{
	m_parent->removeEventFilter(this);
}

void ContextMenuWidget::refreshSelection(const QPoint point)
{
    M3dView view = M3dView::active3dView();
    curve = false;
    keyframe = false;
    frame = false;
    selectedKeys.clear();

	QPoint p = view.widget()->mapFromGlobal(point);
	double y = view.widget()->height() - p.y() - 1;

    // NOTE: The rendererString() method is obsolete. Viewport 2.0 is the default in Maya 2025.
    // The logic is simplified to always use the modern hit-testing path.
    bool new_ = true;
    
    CameraCache *cachePtr = mpManager.getCameraCachePtrFromView(view);
    
	if (new_)
		selectedCurveId = contextUtils::processCurveHits(p.x(), y, GlobalSettings::cameraMatrix, view, cachePtr, mpManager);
	else
		selectedCurveId = contextUtils::processCurveHits(view, cachePtr, mpManager);
    if (selectedCurveId == -1)
        return;
    
    curve = true;
    MotionPath *motionPathPtr = mpManager.getMotionPathPtr(selectedCurveId);
	if (!motionPathPtr)
		return;

	if (new_)
		contextUtils::processKeyFrameHits(p.x(), y, motionPathPtr, view, GlobalSettings::cameraMatrix, cachePtr, selectedKeys);
	else
		contextUtils::processKeyFrameHits(motionPathPtr, view, cachePtr, selectedKeys);
    if (selectedKeys.length() > 0)
    {
        keyframe = true;
        return;
    }
    
	if (new_)
		frame = contextUtils::processFramesHits(p.x(), y, motionPathPtr, view, GlobalSettings::cameraMatrix, cachePtr, frameTime);
	else
		frame = contextUtils::processFramesHits(motionPathPtr, view, cachePtr, frameTime);

}

bool ContextMenuWidget::eventFilter(QObject *o, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonPress)
	{
		QMouseEvent *pMouseEvent = static_cast<QMouseEvent *>(event);
		if((pMouseEvent->button()==Qt::RightButton)&&(pMouseEvent->modifiers()==Qt::NoModifier))
		{
			unsigned int x = pMouseEvent->x(), y = pMouseEvent->y();
            refreshSelection(pMouseEvent->globalPosition().toPoint());
            if (!curve)
                return false;
            
            QPointer<QMenu> menu = new QMenu();
			QPointer<QAction> copyAction = menu->addAction("Copy Selected Keys");
            
            QPointer<QMenu> pasteMenu = menu->addMenu("World Paste");
            QPointer<QAction> pasteAction = pasteMenu->addAction("Paste Here");
            QPointer<QAction> pasteAtCurrentTimeAction = pasteMenu->addAction("Paste At Current Time");
            QPointer<QMenu> offsetPasteMenu = menu->addMenu("Offset Paste");
            QPointer<QAction> offsetPasteAction = offsetPasteMenu->addAction("Paste Here");
            QPointer<QAction> offsetPasteAtCurrentTimeAction = offsetPasteMenu->addAction("Paste At Current Time");
            menu->addSeparator();
            
            QPointer<QAction> addKeyAction = menu->addAction("Add Key");
            QPointer<QAction> deleteKeyAction = menu->addAction("Delete Key");
            QPointer<QAction> deleteSelectedKeysAction = menu->addAction("Delete Selected Keys");
            
            menu->addSeparator();
            
            QPointer<QAction> selectAllKeysAction = menu->addAction("Select All Keys");
            QPointer<QAction> deselectAllKeysAction = menu->addAction("Deselect All Keys");
            QPointer<QAction> invertKeySelectionAction = menu->addAction("Invert Key Selection");
            
            copyAction->setData(QVariant("copy"));
            pasteAction->setData(QVariant("paste"));
            pasteAtCurrentTimeAction->setData("pasteAtCurrentTime");
            addKeyAction->setData(QVariant("addKey"));
            deleteKeyAction->setData(QVariant("deleteKey"));
            offsetPasteAction->setData(QVariant("offsetPaste"));
            offsetPasteAtCurrentTimeAction->setData(QVariant("offsetPasteAtCurrentTime"));
            deleteSelectedKeysAction->setData(QVariant("deleteSelectedKeysAction"));
            selectAllKeysAction->setData(QVariant("selectAllKeysAction"));
            deselectAllKeysAction->setData(QVariant("deselectAllKeysAction"));
            invertKeySelectionAction->setData(QVariant("invertKeySelectionAction"));
            
            MotionPath *motionPathPtr = mpManager.getMotionPathPtr(selectedCurveId);
            bool keySelection = motionPathPtr->getSelectedKeys().length() > 0;
            bool hasCopiedKeys = KeyClipboard::getClipboard().getSize() > 0;
            
            copyAction->setEnabled(keySelection);
            pasteAction->setEnabled(hasCopiedKeys && (frame || keyframe));
            pasteAtCurrentTimeAction->setEnabled(hasCopiedKeys);
            offsetPasteAction->setEnabled(hasCopiedKeys && (frame || keyframe));
            offsetPasteAtCurrentTimeAction->setEnabled(hasCopiedKeys);
            addKeyAction->setEnabled(frame);
            deleteKeyAction->setEnabled(keyframe);
            deleteSelectedKeysAction->setEnabled(keySelection);
            deselectAllKeysAction->setEnabled(keySelection);
            
            menu->popup(m_parent->mapToGlobal(pMouseEvent->pos()));
            
			connect(menu, &QMenu::triggered, this, &ContextMenuWidget::menuAction);
            
			return true;
             
		}
		return false;
	}
	return QWidget::eventFilter(o, event);
}
void ContextMenuWidget::menuAction(QAction* action)
{
    if (action == NULL)  return;

    MotionPath* motionPathPtr = mpManager.getMotionPathPtr(selectedCurveId);
    if (motionPathPtr == NULL) return;

    // 减少重复调用
    QString cmd = action->data().toString();

    if (cmd == "copy")
    {
        motionPathPtr->storeSelectedKeysInClipboard();
        return;
    }

    // 获取场景上的最大时间（MAnimControl::maxTime() 返回 MTime）
    double maxTimeUI = MAnimControl::maxTime().as(MTime::uiUnit());
    double currentTimeUI = MAnimControl::currentTime().as(MTime::uiUnit());

    if (cmd == "offsetPaste")
    {
        if (motionPathPtr->getNumKeyFrames() == 0 && frameTime == maxTimeUI)
            motionPathPtr->pasteKeys(currentTimeUI, true);
        else
            motionPathPtr->pasteKeys(keyframe ? motionPathPtr->getTimeFromKeyId(selectedKeys[0]) : frameTime, true);

        return;
    }

    if (cmd == "offsetPasteAtCurrentTime")
    {
        motionPathPtr->pasteKeys(currentTimeUI, true);
        return;
    }

    if (cmd == "paste")
    {
        if (motionPathPtr->getNumKeyFrames() == 0 && frameTime == maxTimeUI)
            motionPathPtr->pasteKeys(currentTimeUI, false);
        else
            motionPathPtr->pasteKeys(keyframe ? motionPathPtr->getTimeFromKeyId(selectedKeys[0]) : frameTime, false);
        return;
    }

    if (cmd == "pasteAtCurrentTime")
    {
        motionPathPtr->pasteKeys(currentTimeUI, false);
        return;
    }

    if (cmd == "addKey" || cmd == "deleteKey")
    {
        mpManager.startAnimUndoRecording();

        if (cmd == "addKey")
            motionPathPtr->addKeyFrameAtTime(frameTime, mpManager.getAnimCurveChangePtr());
        else
            motionPathPtr->deleteKeyFrameWithId(selectedKeys[0], mpManager.getAnimCurveChangePtr());

        mpManager.stopDGAndAnimUndoRecording();
        M3dView::active3dView().refresh();
        return;
    }

    
    if (action->data().toString() == "deleteSelectedKeysAction")
    {
        mpManager.startAnimUndoRecording();
        
        MDoubleArray sk = motionPathPtr->getSelectedKeys();
        motionPathPtr->deselectAllKeys();
        for (int i = sk.length() - 1; i > -1 ; --i)
            motionPathPtr->deleteKeyFrameAtTime(sk[i], mpManager.getAnimCurveChangePtr());
        
        mpManager.stopDGAndAnimUndoRecording();
        M3dView::active3dView().refresh();
        return;
    }
    
    if (action->data().toString() == "selectAllKeysAction")
    {
        mpManager.storePreviousKeySelection();
        motionPathPtr->selectAllKeys();
        MGlobal::executeCommand("tcMotionPathCmd -keySelectionChanged", true, true);
    }
    
    if (action->data().toString() == "deselectAllKeysAction")
    {
        mpManager.storePreviousKeySelection();
        motionPathPtr->deselectAllKeys();
        MGlobal::executeCommand("tcMotionPathCmd -keySelectionChanged", true, true);
    }
    
    if (action->data().toString() == "invertKeySelectionAction")
    {
        mpManager.storePreviousKeySelection();
        motionPathPtr->invertKeysSelection();
        MGlobal::executeCommand("tcMotionPathCmd -keySelectionChanged", true, true);
    }

	M3dView::active3dView().refresh(true, true);
}

