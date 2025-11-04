// DrawUtils.cpp
// MotionPath - Maya 2025 Compatible Version
// 实现说明见头文件

#include "DrawUtils.h"
#include <cmath>
#include <algorithm>

// 你的工程里需要包含这些实现所依赖的类定义（Keyframe, CameraCache, GlobalSettings）
#include "Keyframe.h"
#include "CameraCache.h"
#include "GlobalSettings.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace drawUtils
{
    // 矩阵向量乘法（column-major / OpenGL 风格兼容）
    void multiplyMatrixVector(const double matrix[16], const double in[4], double out[4])
    {
        out[0] = matrix[0] * in[0] + matrix[4] * in[1] + matrix[8]  * in[2] + matrix[12] * in[3];
        out[1] = matrix[1] * in[0] + matrix[5] * in[1] + matrix[9]  * in[2] + matrix[13] * in[3];
        out[2] = matrix[2] * in[0] + matrix[6] * in[1] + matrix[10] * in[2] + matrix[14] * in[3];
        out[3] = matrix[3] * in[0] + matrix[7] * in[1] + matrix[11] * in[2] + matrix[15] * in[3];
    }

    // projectPoint: 替代 gluProject（ModelView + Projection -> clip -> NDC -> viewport）
    bool projectPoint(double objX, double objY, double objZ,
                      const double modelMatrix[16],
                      const double projMatrix[16],
                      const int viewport[4],
                      double* winX, double* winY, double* winZ)
    {
        double in[4] = { objX, objY, objZ, 1.0 };
        double out[4];

        multiplyMatrixVector(modelMatrix, in, out);

        double clip[4];
        multiplyMatrixVector(projMatrix, out, clip);

        if (clip[3] == 0.0) return false;

        clip[0] /= clip[3];
        clip[1] /= clip[3];
        clip[2] /= clip[3];

        // NDC (-1..1) -> window coords
        *winX = viewport[0] + (1.0 + clip[0]) * viewport[2] / 2.0;
        *winY = viewport[1] + (1.0 + clip[1]) * viewport[3] / 2.0;
        *winZ = (1.0 + clip[2]) / 2.0; // depth 0..1

        return true;
    }

    bool getCurrentGLMatrices(double modelMatrix[16], double projMatrix[16], int viewport[4])
    {
        glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
        glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
        glGetIntegerv(GL_VIEWPORT, viewport);
        return true;
    }

    // ------------ 绘制实现（行为与原版保持一致） ------------
    void drawStippledLineSegments(const MVector &origin, const MVector &target, float lineWidth, const MColor &color)
    {
        MVector direction = target - origin;
        double totalLength = direction.length();
        if (totalLength < 1e-5) return;
        direction.normalize();

        const double dashLength = 8.0;
        const double gapLength  = 8.0;
        const double pattern    = dashLength + gapLength;

        int numSegments = static_cast<int>(std::ceil(totalLength / pattern)) * 2; // conservative
        glLineWidth(lineWidth);
        glColor4d(color.r, color.g, color.b, color.a);

        glBegin(GL_LINES);
        double t = 0.0;
        while (t < totalLength) {
            double start = t;
            double end   = std::min(t + dashLength, totalLength);

            MVector s = origin + direction * start;
            MVector e = origin + direction * end;

            glVertex3d(s.x, s.y, s.z);
            glVertex3d(e.x, e.y, e.z);

            t += pattern;
        }
        glEnd();
    }

    void drawLineStipple(const MVector &origin, const MVector &target, float lineWidth, const MColor &color)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawStippledLineSegments(origin, target, lineWidth, color);
    }

    void drawLine(const MVector &origin, const MVector &target, float lineWidth)
    {
        float prevLineWidth = 1.0f;
        glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
        glLineWidth(lineWidth);

        glBegin(GL_LINES);
        glVertex3d(origin.x, origin.y, origin.z);
        glVertex3d(target.x, target.y, target.z);
        glEnd();

        glLineWidth(prevLineWidth);
    }

    void drawLineWithColor(const MVector &origin, const MVector &target, float lineWidth, const MColor &color)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4d(color.r, color.g, color.b, color.a);
        drawLine(origin, target, lineWidth);
    }

    void drawCirclePoint(const MVector &center, float radius, const MColor &color, int segments)
    {
        glColor4d(color.r, color.g, color.b, color.a);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBegin(GL_TRIANGLE_FAN);
        glVertex3d(center.x, center.y, center.z);
        for (int i = 0; i <= segments; ++i)
        {
            double a = 2.0 * M_PI * i / segments;
            double x = center.x + radius * cos(a);
            double y = center.y + radius * sin(a);
            glVertex3d(x, y, center.z);
        }
        glEnd();
    }

    void drawPoint(const MVector &point, float size)
    {
        float prevSize = 1.0f;
        glGetFloatv(GL_POINT_SIZE, &prevSize);

        glPointSize(size);
        glEnable(GL_POINT_SMOOTH);

        glBegin(GL_POINTS);
        glVertex3d(point.x, point.y, point.z);
        glEnd();

        glPointSize(prevSize);
    }

    void drawPointWithColor(const MVector &point, float size, const MColor &color)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4d(color.r, color.g, color.b, color.a);
        drawPoint(point, size);
    }

    void drawLineArray(const std::vector<MVector> &vertices, float lineWidth, const MColor &color)
    {
        if (vertices.size() < 2) return;

        glLineWidth(lineWidth);
        glColor4d(color.r, color.g, color.b, color.a);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBegin(GL_LINE_STRIP);
        for (size_t i = 0; i < vertices.size(); ++i) {
            glVertex3d(vertices[i].x, vertices[i].y, vertices[i].z);
        }
        glEnd();
    }

    void drawPointArray(const std::vector<MVector> &vertices, float size, const MColor &color)
    {
        if (vertices.empty()) return;

        glPointSize(size);
        glColor4d(color.r, color.g, color.b, color.a);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_POINT_SMOOTH);

        glBegin(GL_POINTS);
        for (size_t i = 0; i < vertices.size(); ++i) {
            glVertex3d(vertices[i].x, vertices[i].y, vertices[i].z);
        }
        glEnd();
    }

    void drawTriangleFan(const MVector &center, float radius, const std::vector<MColor> &sectorColors, int segments)
    {
        if (sectorColors.empty()) return;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        double angleAdd = (2.0 * M_PI) / segments;
        double angle = -M_PI / 2.0;

        int step = segments / static_cast<int>(sectorColors.size());
        int currentColorIndex = 0;

        glBegin(GL_TRIANGLES);

        double x = 0.0;
        double y = radius;

        for (int i = 0; i <= segments; ++i)
        {
            if ((i / step) > currentColorIndex)
            {
                currentColorIndex = i / step;
                if (currentColorIndex >= static_cast<int>(sectorColors.size()))
                    currentColorIndex = 0;
            }

            const MColor &c = sectorColors[currentColorIndex];
            glColor4d(c.r, c.g, c.b, c.a);

            glVertex3d(center.x, center.y, center.z);
            glVertex3d(center.x + x, center.y + y, center.z);

            angle += angleAdd;
            x = radius * sin(angle);
            y = radius * cos(angle);
            glVertex3d(center.x + x, center.y + y, center.z);
        }

        glEnd();
    }

    void setup2DProjection(int width, int height)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
    }

    void restore3DProjection()
    {
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    // drawKeyFrames: 保持原有逻辑与外观
    void drawKeyFrames(std::vector<Keyframe *> keys,
                       const float size,
                       const double colorMultiplier,
                       const int portWidth,
                       const int portHeight,
                       const bool showRotationKeyframes)
    {
        // Performance optimization: Early exit if no keys
        if (keys.empty()) return;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        // Exception safety: Wrap in try-catch to ensure matrix stack is restored
        try
        {
            setup2DProjection(portWidth, portHeight);

        // Performance: Enable blending once for all keyframes
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // First pass: Draw all black background points in batch
        glPointSize(size * GlobalSettings::BLACK_BACKGROUND_FACTOR);
        glColor4d(0.0, 0.0, 0.0, 1.0);
        glEnable(GL_POINT_SMOOTH);
        glBegin(GL_POINTS);
        for (size_t ki = 0; ki < keys.size(); ++ki)
        {
            Keyframe* key = keys[ki];
            if (!key) continue;
            if (key->projPosition.z > 1.0 || key->projPosition.z < 0.0) continue;

            // Fix: Show keyframe if EITHER translation OR rotation has any axis keyed
            std::vector<Keyframe::Axis> tAxis, rAxis;
            key->getKeyTranslateAxis(tAxis);
            key->getKeyRotateAxis(rAxis);
            if (tAxis.size() < 1 && rAxis.size() < 1) continue; // Skip only if BOTH are empty

            double convertY = portHeight - key->projPosition.y;
            glVertex3d(key->projPosition.x, convertY, 0.0);
        }
        glEnd();

        // Second pass: Draw selected keyframes or colored triangles
        for (size_t ki = 0; ki < keys.size(); ++ki)
        {
            Keyframe* key = keys[ki];
            if (!key) continue;
            if (key->projPosition.z > 1.0 || key->projPosition.z < 0.0) continue;

            std::vector<Keyframe::Axis> tAxis, rAxis;
            key->getKeyTranslateAxis(tAxis);
            if (showRotationKeyframes) key->getKeyRotateAxis(rAxis);

            // Fix: Show keyframe if EITHER translation OR rotation has any axis keyed
            if (tAxis.size() < 1 && rAxis.size() < 1) continue; // Skip only if BOTH are empty

            double convertY = portHeight - key->projPosition.y;

            if (key->selectedFromTool)
            {
                // Visual enhancement: Multi-layer selected keyframe
                // Outer glow for better visibility
                glPointSize(size * 1.4f);
                glColor4d(1.0, 1.0, 0.0, 0.5); // Yellow glow
                glBegin(GL_POINTS);
                glVertex3d(key->projPosition.x, convertY, 0.0);
                glEnd();

                // Middle ring
                glPointSize(size * 1.15f);
                glColor4d(1.0, 0.8, 0.0, 0.8); // Orange ring
                glBegin(GL_POINTS);
                glVertex3d(key->projPosition.x, convertY, 0.0);
                glEnd();

                // Bright white center
                glPointSize(size);
                glColor4d(1.0, 1.0, 1.0, 1.0);
                glBegin(GL_POINTS);
                glVertex3d(key->projPosition.x, convertY, 0.0);
                glEnd();
            }
            else
            {
                // Fix: Use combined axis list (translation + rotation) for coloring
                std::vector<Keyframe::Axis> combinedAxis = tAxis;

                // If no translation keys, use rotation keys for coloring
                if (tAxis.size() == 0 && rAxis.size() > 0)
                {
                    combinedAxis = rAxis;
                }

                // Draw colored triangle fan
                int nSections = 12;
                int axisCount = static_cast<int>(combinedAxis.size());
                if (axisCount < 1) axisCount = 1; // Safety: prevent division by zero

                int step = nSections / axisCount;
                int currentStep = 0;
                MColor color;
                double angleAdd = (2.0 * M_PI) / nSections;
                double angle = -M_PI / 2.0;
                float x = 0.0f;
                float y = size / 2.0f;

                glBegin(GL_TRIANGLES);
                for (int i = 0; i <= nSections; ++i)
                {
                    if ((i / step) > currentStep)
                    {
                        currentStep = i / step;
                        if (currentStep >= axisCount)
                            currentStep = 0;
                        Keyframe::getColorForAxis(combinedAxis[currentStep], color);
                        color *= colorMultiplier;
                        glColor4d(color.r, color.g, color.b, color.a);
                    }

                    glVertex3d(key->projPosition.x, convertY, 0.0);
                    glVertex3d(key->projPosition.x + x, convertY + y, 0.0);

                    angle += angleAdd;
                    x = size * 0.5f * static_cast<float>(sin(angle));
                    y = size * 0.5f * static_cast<float>(cos(angle));
                    glVertex3d(key->projPosition.x + x, convertY + y, 0.0);
                }
                glEnd();
            }

            // Draw rotation indicators if present
            if (rAxis.size() > 0)
            {
                double lineWidth = size / 5.0;
                if (lineWidth < 1.0) lineWidth = 1.0;
                double unit = size * GlobalSettings::BLACK_BACKGROUND_FACTOR / 2.0;

                float x1s[3] = {-unit * 0.8f, unit * 1.5f,  unit * -1.5f};
                float y1s[3] = {unit * 1.2f,   unit * 0.1f,  unit * 0.1f};
                float x2s[3] = {unit * 0.8f,   unit * 0.7f,  unit * -0.7f};
                float y2s[3] = {unit * 1.2f,   unit * -1.2f, unit * -1.2f};

                glLineWidth((float)lineWidth);
                glBegin(GL_LINES);
                for (size_t i = 0; i < rAxis.size(); ++i)
                {
                    MColor axisColor;
                    Keyframe::getColorForAxis(rAxis[i], axisColor);
                    axisColor *= colorMultiplier;
                    glColor4d(axisColor.r, axisColor.g, axisColor.b, axisColor.a);

                    float p1x = key->projPosition.x + x1s[i];
                    float p1y = convertY + y1s[i];
                    float p2x = key->projPosition.x + x2s[i];
                    float p2y = convertY + y2s[i];

                    glVertex3d(p1x, p1y, 0.0);
                    glVertex3d(p2x, p2y, 0.0);
                }
                glEnd();
            }
        }

            restore3DProjection();
        }
        catch (...)
        {
            // Silently catch exceptions to ensure matrix stack cleanup
        }

        // Always restore matrix state
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    void drawKeyFramePoints(KeyframeMap &keyframesCache,
                            const float size,
                            const double colorMultiplier,
                            const int portWidth,
                            const int portHeight,
                            const bool showRotationKeyframes)
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        // Exception safety: Ensure matrix stack is always restored
        try
        {
            double modelMatrix[16];
            double projMatrix[16];
            int viewport[4];
            getCurrentGLMatrices(modelMatrix, projMatrix, viewport);

        std::vector<Keyframe*> keys;
        keys.reserve(keyframesCache.size());

        for (KeyframeMapIterator keyIt = keyframesCache.begin(); keyIt != keyframesCache.end(); ++keyIt)
        {
            Keyframe* key = &keyIt->second;
            if (!key) continue;

            projectPoint(key->worldPosition.x,
                         key->worldPosition.y,
                         key->worldPosition.z,
                         modelMatrix, projMatrix, viewport,
                         &key->projPosition.x,
                         &key->projPosition.y,
                         &key->projPosition.z);

            keys.push_back(key);
        }

            drawKeyFrames(keys, size, colorMultiplier, portWidth, portHeight, showRotationKeyframes);
        }
        catch (...)
        {
            // Silently catch exceptions to ensure matrix stack cleanup
        }

        // Always restore matrix state
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    void convertWorldSpaceToCameraSpace(CameraCache* cachePtr,
                                        std::map<double, MPoint> &positions,
                                        std::map<double, MPoint> &screenSpacePositions)
    {
        if (!cachePtr) return;

        double modelMatrix[16];
        double projMatrix[16];
        int viewport[4];
        getCurrentGLMatrices(modelMatrix, projMatrix, viewport);

        for (std::map<double, MPoint>::iterator it = positions.begin(); it != positions.end(); ++it)
        {
            MPoint screen;
            projectPoint(it->second.x, it->second.y, it->second.z,
                         modelMatrix, projMatrix, viewport,
                         &screen.x, &screen.y, &screen.z);
            screenSpacePositions[it->first] = screen;
        }
    }

    void drawFrameLabel(double frame, const MVector &framePos, M3dView &view, const double sizeOffset, const MColor &color, const MMatrix &refMatrix)
    {
        // 使用 view.worldToView & view.viewToWorld 保持原有语义（frame 标签位置计算）
        glColor4d(color.r, color.g, color.b, color.a);

        MVector cameraUp(refMatrix(1, 0), refMatrix(1, 1), refMatrix(1, 2));

        short viewX, viewY;
        view.worldToView(framePos, viewX, viewY);

        MPoint point1;
        MVector vec1;
        // Use frameSize for vertical offset (not sizeOffset which is for font size)
        view.viewToWorld(viewX, viewY + short(GlobalSettings::frameSize), point1, vec1);

        double distance = (framePos - point1).length();
        vec1 *= distance;
        point1 += vec1;
        double up = (framePos - point1).length();

        MString frameStr;
        frameStr = frame;

        MPoint textPos = framePos + (cameraUp * up);

        // Use OpenGL matrix scaling to achieve real text size control
        // Save the current matrix state
        glPushMatrix();

        // Translate to text position
        glTranslated(textPos.x, textPos.y, textPos.z);

        // Scale based on sizeOffset (affects the actual text rendering size)
        // Clamp scale between 0.5x and 10.0x for practical range
        double scale = sizeOffset;
        if (scale < 0.5) scale = 0.5;
        if (scale > 10.0) scale = 10.0;
        glScaled(scale, scale, scale);

        // Translate back to origin for drawText
        glTranslated(-textPos.x, -textPos.y, -textPos.z);

        view.drawText(frameStr, textPos, M3dView::kCenter);

        // Restore the matrix
        glPopMatrix();
    }

} // namespace drawUtils
