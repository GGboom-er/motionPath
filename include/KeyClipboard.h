//
//  KeyClipboard.h
//  MotionPath
//
//  Created by Daniele Federico on 25/01/15.
//  Modified for Maya2025: use SDK types for tangents/weights to avoid overload resolution errors.
//  说明：已统一使用 MFnAnimCurve::TangentValue（通常为 double）以匹配 Maya API 重载签名
//

#ifndef __MotionPath__KeyClipboard__
#define __MotionPath__KeyClipboard__

#include <maya/MVector.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MTime.h>
#include <maya/MMatrix.h>
#include <maya/MAnimCurveChange.h>
#include <maya/MAngle.h>

#include <Keyframe.h>

#include <vector>

class KeyCopy
{
public:
    KeyCopy();

    // copy tangent info from curve for a specific key index and axis
    void copyKeyTangentStatus(MFnAnimCurve &curve, unsigned int keyId, const Keyframe::Axis axis);

    // add a keyframe into three anim curves (x,y,z)
    void addKeyFrame(MFnAnimCurve &cx, MFnAnimCurve &cy, MFnAnimCurve &cz,
                     const MTime &time, const MVector &pos, const bool isBoundary, MAnimCurveChange *change);

    // set tangents for three anim curves (x,y,z)
    void setTangents(MFnAnimCurve &cx, MFnAnimCurve &cy, MFnAnimCurve &cz,
                     const MMatrix &pMatrix, const MTime &time, const bool isBoundary,
                     const bool modifyInTangent, const bool modifyOutTangent,
                     const bool breakTangentsX, const bool breakTangentsY, const bool breakTangentsZ,
                     const bool xWasWeighted, const bool yWasWeighted, const bool zWasWeighted,
                     MAnimCurveChange *change);

    // set tangent helper: value 用 TangentValue，x 用 TangentValue，weight 用 double
    void setTangent(MFnAnimCurve &curve,
                    const MFnAnimCurve::TangentValue value,
                    const unsigned int keyID,
                    const double weight,
                    const MFnAnimCurve::TangentValue x,
                    const bool inTangent,
                    const bool wasWeighted,
                    MAnimCurveChange *change);

    // stored data
    double deltaTime;
    MVector worldPos;
    MVector inWorldTangent;
    MVector outWorldTangent;
    MVector inWeightedWorldTangent;
    MVector outWeightedWorldTangent;

    bool hasKeyX, hasKeyY, hasKeyZ;
    MFnAnimCurve::TangentType tinX, tinY, tinZ;
    MFnAnimCurve::TangentType toutX, toutY, toutZ;
    bool tangentsLockedX, tangentsLockedY, tangentsLockedZ;
    bool weightsLockedX, weightsLockedY, weightsLockedZ;

    // Tangent x components (使用 SDK 的 TangentValue，通常为 double)
    MFnAnimCurve::TangentValue xInX, xOutX, xInY, xOutY, xInZ, xOutZ;

    // weights as double
    double wInX, wOutX, wInY, wOutY, wInZ, wOutZ;
};

class KeyClipboard
{
public:
    static KeyClipboard& getClipboard(){ static KeyClipboard clipboard; return clipboard; }

    void clearClipboard(){ keys.clear(); };
    void setClipboardSize(int size){ keys.reserve(size); };
    void addKey(const KeyCopy &key){ keys.push_back(key); };
    int getSize(){ return static_cast<int>(keys.size()); };
    KeyCopy *keyCopyAt(const int index){ return &keys[index]; };

    void setXWeighted(bool value){ xWeighted = value; };
    void setYWeighted(bool value){ yWeighted = value; };
    void setZWeighted(bool value){ zWeighted = value; };

    bool isXWeighed(){ return xWeighted; };
    bool isYWeighed(){ return yWeighted; };
    bool isZWeighed(){ return zWeighted; };

private:
    KeyClipboard() : xWeighted(false), yWeighted(false), zWeighted(false) {};
    std::vector<KeyCopy> keys;
    bool xWeighted, yWeighted, zWeighted;
};

#endif /* defined(__MotionPath__KeyClipboard__) */
