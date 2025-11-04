// DrawUtils.h
// 兼容 Maya 2025 的 DrawUtils（替代 GLU & 现代化 OpenGL/矩阵投影）
// DrawUtils.h
#ifndef DRAWUTILS_H
#define DRAWUTILS_H

#include <maya/M3dView.h>    // Maya 3D 视图 (M3dView)
#include <maya/MVector.h>    // 向量 (MVector)
#include <maya/MColor.h>     // 颜色 (MColor)
#include <maya/MDagPath.h>
#include <maya/MMatrix.h>    // 矩阵 (MMatrix)
#include <maya/MGlobal.h>
#include <maya/MPoint.h>     // 点 (MPoint)

#include <set>
#include <vector>
#include <map>

// OpenGL headers（跨平台）
#ifdef __APPLE__
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

// 前向声明（与你工程中的真实定义对齐）
class Keyframe;
class CameraCache;
typedef std::map<double, Keyframe> KeyframeMap;
typedef std::map<double, Keyframe>::iterator KeyframeMapIterator;

/*
  说明：
  - 本组件实现不依赖 GLU（替代 gluProject 的 projectPoint 已实现）。
  - 保留原有外部函数签名以保证向后兼容（外部使用者无需修改调用）。
  - 若需要切换到 VP2 的 MUIDrawManager (Viewport 2.0)，后续可把 draw 函数内的 GL 路径替换为 drawManager 调用。
*/

namespace drawUtils
{
    // ============ 数学/投影工具 (project / projection) ============
    // 手动实现投影（替代 gluProject）
    bool projectPoint(double objX, double objY, double objZ,
                      const double modelMatrix[16],
                      const double projMatrix[16],
                      const int viewport[4],
                      double* winX, double* winY, double* winZ);

    // 矩阵向量乘法辅助
    void multiplyMatrixVector(const double matrix[16], const double in[4], double out[4]);

    // ============ 基础绘制 (drawing primitives) ============
    void drawLineStipple(const MVector &origin, const MVector &target, float lineWidth, const MColor &color);
    void drawLine(const MVector &origin, const MVector &target, float lineWidth);
    void drawLineWithColor(const MVector &origin, const MVector &target, float lineWidth, const MColor &color);

    void drawPoint(const MVector &point, float size);
    void drawPointWithColor(const MVector &point, float size, const MColor &color);

    // ============ 高级：关键帧相关 (keyframes) ============
    void drawKeyFrames(std::vector<Keyframe *> keys, const float size, const double colorMultiplier, const int portWidth, const int portHeight, const bool showRotationKeyframes);
    void drawKeyFramePoints(KeyframeMap &keyframesCache, const float size, const double colorMultiplier, const int portWidth, const int portHeight, const bool showRotationKeyframes);

    void convertWorldSpaceToCameraSpace(CameraCache* cachePtr, std::map<double, MPoint> &positions, std::map<double, MPoint> &screenSpacePositions);

    void drawFrameLabel(double frame, const MVector &framePos, M3dView &view, const double sizeOffset, const MColor &color, const MMatrix &refMatrix);

    // ============ 内部辅助（performance-friendly） ============
    void drawStippledLineSegments(const MVector &origin, const MVector &target, float lineWidth, const MColor &color);
    void drawCirclePoint(const MVector &center, float radius, const MColor &color, int segments = 16);
    void drawTriangleFan(const MVector &center, float radius, const std::vector<MColor> &sectorColors, int segments);

    bool getCurrentGLMatrices(double modelMatrix[16], double projMatrix[16], int viewport[4]);

    void setup2DProjection(int width, int height);
    void restore3DProjection();

    void drawLineArray(const std::vector<MVector> &vertices, float lineWidth, const MColor &color);
    void drawPointArray(const std::vector<MVector> &vertices, float size, const MColor &color);
}

#endif // DRAWUTILS_H
