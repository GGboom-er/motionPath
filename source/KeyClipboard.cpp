//
//  KeyClipboard.cpp
//  MotionPath
//
//  Created by Daniele Federico on 27/01/15.
//  Modified for Maya2025: fixed MFnAnimCurve::getTangent/setTangent overload usage and types.
//  重点：使用 MFnAnimCurve::TangentValue 与 double，最后参数传入 true/false（inTangent）以匹配 API。
//

#include "MotionPath.h"
#include "KeyClipboard.h"

KeyCopy::KeyCopy()
{
    deltaTime = 0.0;

    // Tangent x components and weights initialized
    xInX = xOutX = xInY = xOutY = xInZ = xOutZ = 0.0;
    wInX = wOutX = wInY = wOutY = wInZ = wOutZ = 0.0;

    hasKeyX = hasKeyY = hasKeyZ = false;
    tangentsLockedX = tangentsLockedY = tangentsLockedZ = true;
    weightsLockedX = weightsLockedY = weightsLockedZ = true;

    tinX = tinY = tinZ = MFnAnimCurve::kTangentGlobal;
    toutX = toutY = toutZ = MFnAnimCurve::kTangentGlobal;
}

void KeyCopy::copyKeyTangentStatus(MFnAnimCurve &curve, unsigned int keyId, const Keyframe::Axis axis)
{
    bool tangentsLocked = curve.tangentsLocked(keyId);
    bool weightLocked = curve.weightsLocked(keyId);
    MFnAnimCurve::TangentType tin = curve.inTangentType(keyId);
    MFnAnimCurve::TangentType tout = curve.outTangentType(keyId);

    // 保存当前 weighted 状态以便恢复
    bool isWeighted = curve.isWeighted();

    // 使用 SDK 的 TangentValue（通常 double）
    MFnAnimCurve::TangentValue y;

    switch (axis)
    {
        case Keyframe::kAxisX:
        {
            tangentsLockedX = tangentsLocked;
            weightsLockedX = weightLocked;

            // 读取 x,y （以 weighted 模式读取向量形式）
            curve.setIsWeighted(true);
            curve.getTangent(keyId, xInX, y, true);   // in tangent
            curve.getTangent(keyId, xOutX, y, false); // out tangent

            // 读取 angle + weight（非加权形式）
            curve.setIsWeighted(false);
            {
                MAngle angle;
                double w;
                curve.getTangent(keyId, angle, w, true);
                wInX = w;
                curve.getTangent(keyId, angle, w, false);
                wOutX = w;
            }

            tinX = tin;
            toutX = tout;
            break;
        }

        case Keyframe::kAxisY:
        {
            tangentsLockedY = tangentsLocked;
            weightsLockedY = weightLocked;

            curve.setIsWeighted(true);
            curve.getTangent(keyId, xInY, y, true);
            curve.getTangent(keyId, xOutY, y, false);

            curve.setIsWeighted(false);
            {
                MAngle angle;
                double w;
                curve.getTangent(keyId, angle, w, true);
                wInY = w;
                curve.getTangent(keyId, angle, w, false);
                wOutY = w;
            }

            tinY = tin;
            toutY = tout;
            break;
        }

        case Keyframe::kAxisZ:
        {
            tangentsLockedZ = tangentsLocked;
            weightsLockedZ = weightLocked;

            curve.setIsWeighted(true);
            curve.getTangent(keyId, xInZ, y, true);
            curve.getTangent(keyId, xOutZ, y, false);

            curve.setIsWeighted(false);
            {
                MAngle angle;
                double w;
                curve.getTangent(keyId, angle, w, true);
                wInZ = w;
                curve.getTangent(keyId, angle, w, false);
                wOutZ = w;
            }

            tinZ = tin;
            toutZ = tout;
            break;
        }
    }

    // 恢复原始状态
    curve.setIsWeighted(isWeighted);
}

void KeyCopy::addKeyFrame(MFnAnimCurve &cx, MFnAnimCurve &cy, MFnAnimCurve &cz,
                          const MTime &time, const MVector &pos, const bool isBoundary, MAnimCurveChange *change)
{
    unsigned int keyID;

    // X
    if (hasKeyX || isBoundary)
    {
        if (!cx.find(time, keyID))
            keyID = cx.addKey(time, pos.x, tinX, toutX, change);
        else
            cx.setValue(keyID, pos.x, change);

        cx.setTangentsLocked(keyID, false, change);
        cx.setWeightsLocked(keyID, false, change);
    }

    // Y
    if (hasKeyY || isBoundary)
    {
        if (!cy.find(time, keyID))
            keyID = cy.addKey(time, pos.y, tinY, toutY, change);
        else
            cy.setValue(keyID, pos.y, change);

        cy.setTangentsLocked(keyID, false, change);
        cy.setWeightsLocked(keyID, false, change);
    }

    // Z
    if (hasKeyZ || isBoundary)
    {
        if (!cz.find(time, keyID))
            keyID = cz.addKey(time, pos.z, tinZ, toutZ, change);
        else
            cz.setValue(keyID, pos.z, change);

        cz.setTangentsLocked(keyID, false, change);
        cz.setWeightsLocked(keyID, false, change);
    }
}

void KeyCopy::setTangent(MFnAnimCurve &curve,
                         const MFnAnimCurve::TangentValue value,
                         const unsigned int keyID,
                         const double weight,
                         const MFnAnimCurve::TangentValue x,
                         const bool inTangent,
                         const bool wasWeighted,
                         MAnimCurveChange *change)
{
    // 如果曲线不是 weighted，则使用 angle + weight 形式
    if (!curve.isWeighted())
    {
        // value 存储为 tangent vector 的 y 比例，weight 为 double
        MAngle angle(atan(static_cast<double>(value) * weight));
        curve.setTangent(keyID, angle, static_cast<double>(weight), inTangent, change);
    }
    else
    {
        // weighted 曲线使用 x,y 形式（TangentValue）
        MFnAnimCurve::TangentValue y = static_cast<MFnAnimCurve::TangentValue>(value * static_cast<MFnAnimCurve::TangentValue>(3.0));

        // 把 x 从 seconds 转为当前 UI 时间单位（保持和原逻辑一致）
        MTime convert(1.0, MTime::kSeconds);
        MFnAnimCurve::TangentValue _x = static_cast<MFnAnimCurve::TangentValue>(x * static_cast<MFnAnimCurve::TangentValue>(convert.as(MTime::uiUnit())));

        curve.setTangent(keyID, _x, y, inTangent, change);
    }
}

void KeyCopy::setTangents(MFnAnimCurve &cx, MFnAnimCurve &cy, MFnAnimCurve &cz,
                          const MMatrix &pMatrix, const MTime &time, const bool isBoundary,
                          const bool modifyInTangent, const bool modifyOutTangent,
                          const bool breakTangentsX, const bool breakTangentsY, const bool breakTangentsZ,
                          const bool xWasWeighted, const bool yWasWeighted, const bool zWasWeighted, MAnimCurveChange *change)
{
    unsigned int keyID;
    MVector in = (inWorldTangent - worldPos) * pMatrix;
    MVector out = (outWorldTangent - worldPos) * pMatrix;
    MVector inWeighted = (inWeightedWorldTangent - worldPos) * pMatrix;
    MVector outWeighted = (outWeightedWorldTangent - worldPos) * pMatrix;

    MFnAnimCurve::TangentValue inValue, outValue;

    // X
    if (hasKeyX || isBoundary)
    {
        if (cx.isWeighted())
        {
            inValue = static_cast<MFnAnimCurve::TangentValue>(inWeighted.x);
            outValue = static_cast<MFnAnimCurve::TangentValue>(outWeighted.x);
        }
        else
        {
            inValue = static_cast<MFnAnimCurve::TangentValue>(in.x);
            outValue = static_cast<MFnAnimCurve::TangentValue>(out.x);
        }

        if (cx.find(time, keyID))
        {
            if (modifyInTangent)
                setTangent(cx, -inValue, keyID, wInX, xInX, true, xWasWeighted, change);
            if (modifyOutTangent)
                setTangent(cx, outValue, keyID, wOutX, xOutX, false, xWasWeighted, change);

            if (breakTangentsX)
                cx.setTangentsLocked(keyID, false, change);
            else
                cx.setTangentsLocked(keyID, tangentsLockedX, change);
            cx.setWeightsLocked(keyID, weightsLockedX, change);
        }
    }

    // Y
    if (hasKeyY || isBoundary)
    {
        if (cy.isWeighted())
        {
            inValue = static_cast<MFnAnimCurve::TangentValue>(inWeighted.y);
            outValue = static_cast<MFnAnimCurve::TangentValue>(outWeighted.y);
        }
        else
        {
            inValue = static_cast<MFnAnimCurve::TangentValue>(in.y);
            outValue = static_cast<MFnAnimCurve::TangentValue>(out.y);
        }

        if (cy.find(time, keyID))
        {
            if (modifyInTangent)
                setTangent(cy, -inValue, keyID, wInY, xInY, true, yWasWeighted, change);
            if (modifyOutTangent)
                setTangent(cy, outValue, keyID, wOutY, xOutY, false, yWasWeighted, change);

            if (breakTangentsY)
                cy.setTangentsLocked(keyID, false, change);
            else
                cy.setTangentsLocked(keyID, tangentsLockedY, change);
            cy.setWeightsLocked(keyID, weightsLockedY, change);
        }
    }

    // Z
    if (hasKeyZ || isBoundary)
    {
        if (cz.isWeighted())
        {
            inValue = static_cast<MFnAnimCurve::TangentValue>(inWeighted.z);
            outValue = static_cast<MFnAnimCurve::TangentValue>(outWeighted.z);
        }
        else
        {
            inValue = static_cast<MFnAnimCurve::TangentValue>(in.z);
            outValue = static_cast<MFnAnimCurve::TangentValue>(out.z);
        }

        if (cz.find(time, keyID))
        {
            if (modifyInTangent)
                setTangent(cz, -inValue, keyID, wInZ, xInZ, true, zWasWeighted, change);
            if (modifyOutTangent)
                setTangent(cz, outValue, keyID, wOutZ, xOutZ, false, zWasWeighted, change);

            if (breakTangentsZ)
                cz.setTangentsLocked(keyID, false, change);
            else
                cz.setTangentsLocked(keyID, tangentsLockedZ, change);
            cz.setWeightsLocked(keyID, weightsLockedZ, change);
        }
    }
}
