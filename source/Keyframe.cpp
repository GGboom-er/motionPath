#include "Keyframe.h"
#include <cmath> // for tan()

Keyframe::Keyframe()
{
    this->id = -1;
    this->time = 0.0;
    this->tangentsLocked = true;
    this->xKeyId = -1;
    this->yKeyId = -1;
    this->zKeyId = -1;
    this->xRotKeyId = -1;
    this->yRotKeyId = -1;
    this->zRotKeyId = -1;
    this->showInTangent = true;
    this->showOutTangent = true;
    this->selectedFromTool = false;
}


bool Keyframe::hasTranslationXYZ()
{
    return (this->xKeyId != -1 && this->yKeyId != -1 && this->zKeyId != -1);
}

bool Keyframe::hasRotationXYZ()
{
    return (this->xRotKeyId != -1 && this->yRotKeyId != -1 && this->zRotKeyId != -1);
}

void Keyframe::setTangentValue(double value, const Keyframe::Axis& axisName, const Keyframe::Tangent& tangentName)
{
    switch (axisName)
    {
    case Keyframe::kAxisX:
        if (tangentName == Keyframe::kInTangent)
            this->inTangent.x = value;
        else
            this->outTangent.x = value;
        break;

    case Keyframe::kAxisY:
        if (tangentName == Keyframe::kInTangent)
            this->inTangent.y = value;
        else
            this->outTangent.y = value;
        break;

    case Keyframe::kAxisZ:
        if (tangentName == Keyframe::kInTangent)
            this->inTangent.z = value;
        else
            this->outTangent.z = value;
        break;
    }
}

// Note: Parameter 'curve' is non-const to allow calling non-const APIs of MFnAnimCurve
void Keyframe::setTangent(int keyIndex, MFnAnimCurve& curve, const Keyframe::Axis& axisName, const Keyframe::Tangent& tangentName)
{
    double tangentVal = 0.0;

    // Follow Maya API convention: use unsigned int for key index
    unsigned int uKey = static_cast<unsigned int>(keyIndex);

    // Convert enum to bool
    bool isIn = (tangentName == Keyframe::kInTangent);

    if (!curve.isWeighted())
    {
        MAngle angle;
        double w1 = 0.0;
        MStatus st = curve.getTangent(uKey, angle, w1, isIn);
        if (!st)
        {
            tangentVal = 0.0;
        }
        else
        {
            tangentVal = std::tan(angle.asRadians()) * w1;
        }
    }
    else
    {
        MFnAnimCurve::TangentValue x = 0.0, y = 0.0;
        MStatus st = curve.getTangent(uKey, x, y, isIn);
        if (!st)
        {
            tangentVal = 0.0;
        }
        else
        {
            tangentVal = static_cast<double>(y / static_cast<MFnAnimCurve::TangentValue>(3.0)); // Maintain original logic with consistency
        }
    }

    this->setTangentValue(tangentVal, axisName, tangentName);
}


void Keyframe::setKeyId(int id, const Keyframe::Axis& axisName)
{
    switch (axisName)
    {
    case Keyframe::kAxisX:
        this->xKeyId = id;
        break;

    case Keyframe::kAxisY:
        this->yKeyId = id;
        break;

    case Keyframe::kAxisZ:
        this->zKeyId = id;
        break;
    }
}

void Keyframe::setRotKeyId(int id, const Keyframe::Axis& axisName)
{
    switch (axisName)
    {
    case Keyframe::kAxisX:
        this->xRotKeyId = id;
        break;

    case Keyframe::kAxisY:
        this->yRotKeyId = id;
        break;

    case Keyframe::kAxisZ:
        this->zRotKeyId = id;
        break;
    }
}

void Keyframe::getKeyTranslateAxis(std::vector<Keyframe::Axis>& axis)
{
    if (xKeyId != -1)
        axis.push_back(Keyframe::kAxisX);

    if (yKeyId != -1)
        axis.push_back(Keyframe::kAxisY);

    if (zKeyId != -1)
        axis.push_back(Keyframe::kAxisZ);
}

void Keyframe::getKeyRotateAxis(std::vector<Keyframe::Axis>& axis)
{
    if (xRotKeyId != -1)
        axis.push_back(Keyframe::kAxisX);

    if (yRotKeyId != -1)
        axis.push_back(Keyframe::kAxisY);

    if (zRotKeyId != -1)
        axis.push_back(Keyframe::kAxisZ);
}

void Keyframe::getColorForAxis(const Keyframe::Axis axis, MColor& color)
{
    switch (axis)
    {
    case Keyframe::kAxisX:
        color.r = 1.0; color.g = 0.0; color.b = 0.0;
        break;

    case Keyframe::kAxisY:
        color.r = 0.0; color.g = 1.0; color.b = 0.0;
        break;

    case Keyframe::kAxisZ:
        color.r = 0.0; color.g = 0.0; color.b = 1.0;
        break;
    }
}
