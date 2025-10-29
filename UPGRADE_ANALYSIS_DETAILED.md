# Maya Motion Path 插件升级到 Maya 2025 - 详细技术分析报告

**版本**: 1.0
**目标Maya版本**: Maya 2025
**当前支持**: Maya 2015
**分析日期**: 2025-10-29
**代码总行数**: 8,177行

---

## 📑 目录

1. [执行摘要](#执行摘要)
2. [代码架构分析](#代码架构分析)
3. [VS2022编译错误分析](#vs2022编译错误分析)
4. [Maya API过时问题](#maya-api过时问题)
5. [C++17兼容性问题](#c17兼容性问题)
6. [完整修复清单](#完整修复清单)
7. [升级实施计划](#升级实施计划)

---

## 执行摘要

### 插件功能概述
tcMotionPath是一个Maya动画路径可视化和编辑插件，提供：
- ✅ 实时3D运动路径显示
- ✅ 交互式关键帧编辑
- ✅ 切线可视化和编辑
- ✅ 世界空间/相机空间模式
- ✅ 缓冲路径系统
- ✅ 关键帧复制/粘贴
- ✅ 描边模式重新分布

### 关键发现
| 问题类型 | 数量 | 严重性 | 工作量估计 |
|---------|------|--------|-----------|
| Maya API过时调用 | 20+ | 🔴 高 | 4-6小时 |
| VS2022编译错误 | 10+ | 🔴 高 | 3-4小时 |
| Windows宏冲突 | 潜在 | 🟡 中 | 1-2小时 |
| OpenGL过时调用 | 100+ | 🟡 中 | 可选优化 |
| C++17兼容性 | 若干 | 🟢 低 | 1-2小时 |

**总预估工作量**: 10-15小时

---

## 代码架构分析

### 核心组件

#### 1. 插件入口 (PluginMain.cpp - 222行)
```cpp
MStatus initializePlugin(MObject obj)
{
    MFnPlugin plugin(obj, "ToolChefs_MotionPath", "1.0", "Any");

    // 注册3个命令
    plugin.registerContextCommand("tcMotionPathEditContext", ...);
    plugin.registerContextCommand("tcMotionPathDrawContext", ...);
    plugin.registerCommand("tcMotionPathCmd", ...);

    // 注册VP2.0渲染覆盖
    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    MotionPathRenderOverride *overridePtr = new MotionPathRenderOverride(...);
    renderer->registerOverride(overridePtr);

    // 创建MEL菜单和工具架按钮
    MGlobal::executeCommand(addMenu, false, false);
}
```

**功能**:
- 插件初始化和清理
- 命令注册
- VP2.0渲染器集成
- UI集成（菜单、工具架）

#### 2. MotionPathManager (799行 + 156行头文件)

**职责**:
- 管理所有MotionPath对象
- 视口回调管理
- 相机缓存系统
- 事件处理（选择变化、时间变化等）

**关键回调**:
```cpp
// 时间变化
MEventMessage::addEventCallback("timeChanged", timeChangeEvent, ...);

// 选择变化
MModelMessage::addCallback(MModelMessage::kActiveListModified, selectionChangeCallback, ...);

// 视口渲染
MUiMessage::add3dViewPostRenderMsgCallback(panelName, viewPostRenderCallback, ...);

// 相机变化
MUiMessage::addCameraChangedCallback(panelName, viewCameraChanged, ...);
```

**相机缓存系统**:
```cpp
class CameraCache {
    std::map<double, MMatrix> matrixCache;  // 时间->矩阵缓存
    int portWidth, portHeight;
    MPlug worldMatrixPlug;
    MPlug txPlug, tyPlug, tzPlug;
    MPlug rxPlug, ryPlug, rzPlug;
};

typedef std::map<std::string, CameraCache> CameraCacheMap;
```

#### 3. MotionPath (1991行 + 197行头文件)

**核心数据结构**:
```cpp
class MotionPath {
private:
    MObject thisObject;
    MPlug txPlug, tyPlug, tzPlug;  // 平移属性
    MPlug rxPlug, ryPlug, rzPlug;  // 旋转属性

    // 关键帧缓存
    KeyframeMap keyframesCache;  // std::map<double, Keyframe>

    // 父矩阵缓存
    std::map<double, MMatrix> pMatrixCache;
    MPlug pMatrixPlug;

    // 选择状态
    std::set<double> selectedKeyTimes;

    bool constrained;  // 是否有约束
    bool cacheDone;
    bool isWeighted;   // 是否加权
};
```

**关键功能**:
1. **关键帧缓存** (expandKeyFramesCache)
   - 从动画曲线提取关键帧
   - 计算世界空间位置
   - 存储切线信息

2. **父矩阵缓存** (cacheParentMatrixRange)
   - 缓存时间范围内的父变换
   - 支持约束对象

3. **绘制** (draw, drawPath, drawKeyFrames)
   - 双管线支持：OpenGL + VP2.0
   - 世界空间/相机空间转换

4. **编辑操作**:
   - setFrameWorldPosition - 移动关键帧
   - setTangentWorldPosition - 编辑切线
   - addKeyFrameAtTime - 添加关键帧
   - deleteKeyFrameAtTime - 删除关键帧

#### 4. 交互上下文

**MotionPathEditContext** (394行 + 99行头文件)
```cpp
class MotionPathEditContext : public MPxContext {
    enum EditMode {
        kNoneMode = 0,
        kFrameEditMode = 1,      // 编辑关键帧位置
        kTangentEditMode = 2,    // 编辑切线
        kShiftKeyMode = 3        // Shift键模式
    };

    virtual MStatus doPress(MEvent &event);
    virtual MStatus doDrag(MEvent &event);
    virtual MStatus doRelease(MEvent &event);
};
```

**MotionPathDrawContext** (617行 + 102行头文件)
```cpp
class MotionPathDrawContext : public MPxContext {
    enum DrawMode {
        kNoneMode = 0,
        kClickAddWorld = 1,  // 点击添加
        kDraw = 2,           // 绘制
        kStroke = 3          // 描边模式
    };

    MVectorArray strokePoints;  // 描边点
};
```

**描边模式实现**:
- Ctrl+拖动关键帧激活
- Closest模式：移动关键帧到描边最近点
- Spread模式：沿描边均匀分布关键帧

#### 5. 渲染系统

**双管线支持**:

1. **Legacy OpenGL** (DrawUtils.cpp - 597行)
```cpp
namespace drawUtils {
    void drawLine(const MVector &origin, const MVector &target, float lineWidth) {
        glLineWidth(lineWidth);
        glBegin(GL_LINES);
        glVertex3f(origin.x, origin.y, origin.z);
        glVertex3f(target.x, target.y, target.z);
        glEnd();
    }
}
```

2. **Viewport 2.0** (Vp2DrawUtils.cpp - 236行)
```cpp
namespace VP2DrawUtils {
    void drawLine(const MVector &origin, const MVector &target,
                  float lineWidth, const MColor &color,
                  const MMatrix &cameraMatrix,
                  MHWRender::MUIDrawManager* drawManager,
                  const MHWRender::MFrameContext* frameContext) {
        drawManager->setColor(color);
        drawManager->setLineWidth(lineWidth);
        drawManager->line(MPoint(origin), MPoint(target));
    }
}
```

**MotionPathRenderOverride** (116行)
```cpp
class MotionPathRenderOverride : public MHWRender::MRenderOverride {
    MotionPathUserOperation* mMotionPathOp;  // 自定义渲染操作

    virtual MStatus setup(const MString & destination);
    virtual MStatus cleanup();
};

class MotionPathUserOperation : public MHWRender::MHUDRender {
    virtual void addUIDrawables(
        MHWRender::MUIDrawManager& drawManager,
        const MHWRender::MFrameContext& frameContext);
};
```

#### 6. 数据结构

**Keyframe** (158行 + 76行头文件)
```cpp
class Keyframe {
public:
    int id;
    double time;

    // 多空间坐标
    MVector position;       // 本地空间
    MVector worldPosition;  // 世界空间
    MVector projPosition;   // 投影空间

    // 切线
    MVector inTangent, outTangent;
    MVector inTangentWorld, outTangentWorld;
    bool tangentsLocked;
    bool showInTangent, showOutTangent;

    // 轴索引
    int xKeyId, yKeyId, zKeyId;          // 平移
    int xRotKeyId, yRotKeyId, zRotKeyId; // 旋转

    bool selectedFromTool;
};

typedef std::map<double, Keyframe> KeyframeMap;
```

**BufferPath** (140行 + 52行头文件)
```cpp
class BufferPath {
private:
    std::vector<MVector> frames;           // 每帧位置
    std::map<double, MVector> keyFrames;   // 关键帧位置
    bool selected;
    double minTime;

public:
    void draw(M3dView &view, CameraCache* cachePtr, ...);
    void setFrames(std::vector<MVector> value);
    void setKeyFrames(std::map<double, MVector> value);
};
```

#### 7. 工具类

**animCurveUtils** (46行 + 25行头文件)
```cpp
namespace animCurveUtils {
    // 更新曲线，保存旧值
    bool updateCurve(const MPlug &plug, MFnAnimCurve &curve,
                     const MTime &currentTime,
                     double &oldValue, double &newValue,
                     int &newKeyId, int &oldKeyId);

    // 恢复曲线到旧值
    void restoreCurve(MFnAnimCurve &curve, const MTime &currentTime,
                      const double oldValue, const int newKeyId,
                      const int oldKeyId);
}
```

**KeyClipboard** (246行 + 76行头文件)
- 复制/粘贴关键帧的3D位置
- 支持世界空间和偏移粘贴
- 存储切线状态

**GlobalSettings** (35行 + 51行头文件)
```cpp
class GlobalSettings {
public:
    static double startTime, endTime;
    static double framesBack, framesFront;
    static MColor pathColor, currentFrameColor, tangentColor;
    static bool showTangents, showKeyFrames, showRotationKeyFrames;
    static bool lockedMode, enabled;
    static DrawMode motionPathDrawMode;
    // ... 更多设置
};
```

---

## VS2022编译错误分析

### 问题1: C2568 - 无法解析函数重载

**错误位置**: MotionPath.cpp:466, 449

```cpp
// 第448-449行
double endTime = isDrawing ? endDrawingTime : displayEndTime;
```

**根本原因**:
这不是真正的重载问题，而是Windows.h中的min/max宏冲突。当包含Windows头文件时：
```cpp
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
```

这些宏会干扰三元运算符的解析。

**解决方案**:
```cpp
// 方案1: 在所有源文件顶部添加
#define NOMINMAX

// 方案2: 使用括号保护
double endTime = (isDrawing) ? (endDrawingTime) : (displayEndTime);

// 方案3: 使用临时变量
bool drawing = this->isDrawing;
double endDraw = this->endDrawingTime;
double endDisplay = this->displayEndTime;
double endTime = drawing ? endDraw : endDisplay;
```

### 问题2: C2511 - 没有找到重载的成员函数

**错误位置**: MotionPath.cpp:445

```cpp
void MotionPath::expandKeyFramesCache(const MFnAnimCurve &curve,
                                       const Keyframe::Axis &axisName,
                                       bool isTranslate)
```

**可能原因**:
1. **const引用问题**: MFnAnimCurve在某些版本中可能不支持const引用
2. **命名空间问题**: Maya 2025的MFnAnimCurve可能在不同命名空间

**解决方案**:
```cpp
// 修改头文件和实现，去掉const
void expandKeyFramesCache(MFnAnimCurve &curve,
                          const Keyframe::Axis &axisName,
                          bool isTranslate);
```

### 问题3: C2597 - 对非静态成员的非法引用

**错误位置**: MotionPath.cpp:449, 456, 466

**根本原因**: 同问题1，宏冲突导致编译器误解析

### 问题4: C3867 - 非标准语法；请使用"&"来创建指向成员的指针

**根本原因**: 同问题1，宏冲突

### 问题5: C2440 - 无法从"unknown"转换为"bool"

**根本原因**: 同问题1，宏冲突导致类型推断失败

---

## Maya API过时问题

### 🔴 严重问题1: MFnDependencyNode::findPlug() 单参数版本

**影响范围**: 20处调用

**位置清单**:
```
source/MotionPath.cpp:33-47      (10处 - 构造函数)
source/MotionPath.cpp:109-115    (6处 - cacheParentMatrixRangeForWorldCallback)
source/MotionPath.cpp:225-227    (3处 - worldMatrixChangedCallback)
source/MotionPath.cpp:307        (1处 - findParentMatrixPlug)
source/CameraCache.cpp:29        (1处 - initialize)
source/CameraCache.cpp:41-47     (7处 - initialize)
source/MotionPathManager.cpp:494 (1处 - hasSelectionListChanged)
```

**当前代码**:
```cpp
// ❌ Maya 2016+已弃用
MPlug txPlug = depNodFn.findPlug("translateX");
```

**Maya 2025正确语法**:
```cpp
// ✅ 方法1: 添加参数
MStatus status;
MPlug txPlug = depNodFn.findPlug("translateX", false, &status);
// 参数: wantNetworkedPlug=false (不使用遗留行为)

// ✅ 方法2: 使用静态属性 (更推荐)
MPlug txPlug = depNodFn.findPlug(MFnTransform::translateX, &status);
```

**修复策略**:
1. 所有字符串查找改为双参数版本
2. 添加错误检查
3. 关键位置使用静态属性

**示例修复**:
```cpp
// 修复前
MFnDependencyNode depNodFn(object);
txPlug = depNodFn.findPlug("translateX");
tyPlug = depNodFn.findPlug("translateY");
tzPlug = depNodFn.findPlug("translateZ");

// 修复后
MFnDependencyNode depNodFn(object);
MStatus status;

txPlug = depNodFn.findPlug("translateX", false, &status);
CHECK_MSTATUS(status);

tyPlug = depNodFn.findPlug("translateY", false, &status);
CHECK_MSTATUS(status);

tzPlug = depNodFn.findPlug("translateZ", false, &status);
CHECK_MSTATUS(status);

// 或使用宏简化
#define FIND_PLUG(node, attrName, plug) \
    { MStatus st; plug = node.findPlug(attrName, false, &st); CHECK_MSTATUS(st); }

FIND_PLUG(depNodFn, "translateX", txPlug);
FIND_PLUG(depNodFn, "translateY", tyPlug);
FIND_PLUG(depNodFn, "translateZ", tzPlug);
```

### 🔴 严重问题2: MPlug::evaluateNumElements()

**影响范围**: 2处调用

**位置**:
```
source/MotionPath.cpp:308        (findParentMatrixPlug)
source/CameraCache.cpp:30        (initialize)
```

**当前代码**:
```cpp
// ❌ 已弃用
MPlug parentMatrixPlugs = dagNodeFn.findPlug("parentMatrix");
parentMatrixPlugs.evaluateNumElements();  // 强制计算元素数量
matrixPlug = parentMatrixPlugs[0];
```

**Maya 2025正确语法**:
```cpp
// ✅ 新方法
MPlug parentMatrixPlugs = dagNodeFn.findPlug("parentMatrix", false);
unsigned int numElements = parentMatrixPlugs.numElements();  // 直接获取
matrixPlug = parentMatrixPlugs.elementByPhysicalIndex(0);

// 或者如果只需要第一个元素
MPlug matrixPlug = parentMatrixPlugs.elementByLogicalIndex(0);
```

**修复示例**:
```cpp
// 文件: source/MotionPath.cpp:304-310
// 修复前
void MotionPath::findParentMatrixPlug(const MObject &transform,
                                       const bool constrained,
                                       MPlug &matrixPlug)
{
    MFnDagNode dagNodeFn(transform);
    MPlug parentMatrixPlugs = dagNodeFn.findPlug(constrained ? "worldMatrix" : "parentMatrix");
    parentMatrixPlugs.evaluateNumElements();
    matrixPlug = parentMatrixPlugs[0];
}

// 修复后
void MotionPath::findParentMatrixPlug(const MObject &transform,
                                       const bool constrained,
                                       MPlug &matrixPlug)
{
    MFnDagNode dagNodeFn(transform);
    MStatus status;

    const char* attrName = constrained ? "worldMatrix" : "parentMatrix";
    MPlug parentMatrixPlugs = dagNodeFn.findPlug(attrName, false, &status);
    CHECK_MSTATUS(status);

    // 使用新API获取第一个元素
    matrixPlug = parentMatrixPlugs.elementByLogicalIndex(0, &status);
    CHECK_MSTATUS(status);
}
```

### 🟡 中等问题3: M3dView::drawText

**影响范围**: 若干处

**位置**:
```
source/DrawUtils.cpp:314         (drawFrameLabel)
```

**当前代码**:
```cpp
// Legacy Viewport方法
view.drawText(frameStr, textPos, M3dView::kCenter);
```

**VP2.0方法** (已实现):
```cpp
// VP2DrawUtils.cpp中已有实现
drawManager->text(textPos, frameStr,
                  MHWRender::MUIDrawManager::kCenter);
```

**修复策略**:
- 保留M3dView::drawText作为回退
- 优先使用VP2.0路径（已实现）

### 🟡 中等问题4: 直接OpenGL调用

**影响范围**: 约100+调用

**位置**: 整个DrawUtils.cpp (597行)

**当前实现**:
```cpp
void drawUtils::drawLine(const MVector &origin, const MVector &target,
                         float lineWidth)
{
    glLineWidth(lineWidth);
    glBegin(GL_LINES);
    glVertex3f(origin.x, origin.y, origin.z);
    glVertex3f(target.x, target.y, target.z);
    glEnd();
}
```

**影响**:
- Maya 2025可能禁用Legacy Viewport
- OpenGL调用在VP2.0中性能较差

**当前状态**:
- ✅ 已有VP2DrawUtils完整实现
- ✅ 已有MotionPathRenderOverride
- ✅ 代码已经支持双管线

**修复策略**:
- 保留OpenGL代码（向后兼容）
- 默认使用VP2.0路径
- 仅在VP2.0不可用时回退

---

## C++17兼容性问题

### 问题1: Windows.h宏冲突 🔴 高优先级

**问题**: min/max宏污染全局命名空间

**解决方案**: 在所有.cpp文件开头添加
```cpp
// 放在所有#include之前
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN  // 可选，减少Windows.h包含内容

#include <maya/...>
```

或在项目设置中全局定义NOMINMAX

### 问题2: std::min/std::max使用

**当前代码**:
```cpp
// 可能存在的裸min/max调用
int result = min(a, b);  // ❌ 宏冲突
```

**修复**:
```cpp
#include <algorithm>
int result = std::min(a, b);  // ✅ 使用std版本
```

### 问题3: Qt 5 -> Qt 6

**当前**:
```cpp
#include <QtWidgets/QApplication>  // Qt 5.x
```

**Maya 2025**: Qt 6.x

**影响**: 大部分Qt 5代码兼容，但需要更新链接

**修复**: Makefile中更新Qt路径

### 问题4: C++标准

**当前Makefile**:
```makefile
CFLAGS = -m64 -pthread -pipe -D_BOOL -DLINUX ...
```

**Maya 2025要求**:
```makefile
CFLAGS = -std=c++17 -m64 -pthread -pipe -D_BOOL -DLINUX -DNOMINMAX ...
```

---

## 完整修复清单

### 阶段1: 必须修复（编译通过） ⭐⭐⭐⭐⭐

#### 1.1 Windows宏冲突
- [ ] 所有.cpp文件添加 `#define NOMINMAX`
- [ ] 修复MotionPath.cpp:448行三元运算符
- [ ] 检查所有min/max使用

**文件列表**:
```
source/MotionPath.cpp
source/MotionPathManager.cpp
source/MotionPathEditContext.cpp
source/MotionPathDrawContext.cpp
source/DrawUtils.cpp
source/Vp2DrawUtils.cpp
source/MotionPathCmd.cpp
source/CameraCache.cpp
source/BufferPath.cpp
source/Keyframe.cpp
source/KeyClipboard.cpp
source/ContextUtils.cpp
source/GlobalSettings.cpp
source/animCurveUtils.cpp
source/MotionPathOverride.cpp
source/PluginMain.cpp
source/MotionPathEditContextMenuWidget.cpp
```

#### 1.2 findPlug API更新
- [ ] MotionPath.cpp:33-47 (10处)
- [ ] MotionPath.cpp:109-115 (6处)
- [ ] MotionPath.cpp:225-227 (3处)
- [ ] MotionPath.cpp:307 (1处)
- [ ] CameraCache.cpp:29 (1处)
- [ ] CameraCache.cpp:41-47 (7处)
- [ ] MotionPathManager.cpp:494 (1处)

**总计**: 29处修改

#### 1.3 evaluateNumElements API更新
- [ ] MotionPath.cpp:308
- [ ] CameraCache.cpp:30

#### 1.4 const MFnAnimCurve引用问题
- [ ] 检查所有MFnAnimCurve参数
- [ ] 移除不必要的const限定符

**可能受影响的函数**:
```cpp
// include/MotionPath.h
void expandKeyFramesCache(const MFnAnimCurve &curve, ...);  // 移除const
void cacheKeyFrames(const MFnAnimCurve &curveTX, ...);      // 移除const
void setShowInOutTangents(const MFnAnimCurve &curveTX, ...);// 移除const
```

#### 1.5 Makefile更新
```makefile
# 修改前
C++ = g++412
MAYAVERSION = 2015
CFLAGS = -m64 -pthread -pipe -D_BOOL -DLINUX -DREQUIRE_IOSTREAM -Wno-deprecated -fno-gnu-keywords -fPIC

# 修改后
C++ = g++
MAYAVERSION = 2025
CFLAGS = -std=c++17 -m64 -pthread -pipe -D_BOOL -DLINUX -DREQUIRE_IOSTREAM -DNOMINMAX -Wno-deprecated -fno-gnu-keywords -fPIC

MAYA_LOCATION = /usr/autodesk/maya$(MAYAVERSION)
```

### 阶段2: 警告处理（代码质量） ⭐⭐⭐⭐

#### 2.1 添加错误检查
```cpp
// 所有API调用添加状态检查
MStatus status;
MPlug plug = node.findPlug("attr", false, &status);
if (status != MS::kSuccess) {
    MGlobal::displayError("Failed to find plug: attr");
    return;
}
```

#### 2.2 类型转换警告
- [ ] 检查所有int/unsigned int转换
- [ ] 检查float/double隐式转换
- [ ] 修复所有编译器警告

#### 2.3 弃用警告
- [ ] 替换所有弃用的Maya API调用

### 阶段3: 优化（可选） ⭐⭐⭐

#### 3.1 使用静态属性
```cpp
// 替代字符串查找
MPlug txPlug = depNodFn.findPlug(MFnTransform::translateX, &status);
```

#### 3.2 VP2.0优化
- [ ] 考虑移除OpenGL回退代码
- [ ] 优化VP2.0渲染性能

#### 3.3 现代C++特性
- [ ] 使用auto关键字
- [ ] 使用智能指针（谨慎）
- [ ] 使用range-based for循环

---

## 升级实施计划

### Week 1: 核心修复

**Day 1-2: 宏冲突和API更新**
- [ ] 添加NOMINMAX到所有文件
- [ ] 更新所有findPlug调用
- [ ] 更新evaluateNumElements

**Day 3: 编译和调试**
- [ ] 首次编译
- [ ] 修复编译错误
- [ ] 修复链接错误

**Day 4-5: 测试**
- [ ] 加载测试
- [ ] 功能测试
- [ ] 修复运行时错误

### Week 2: 质量和优化

**Day 1-2: 警告处理**
- [ ] 修复所有编译警告
- [ ] 添加错误检查
- [ ] 代码审查

**Day 3-4: 测试和文档**
- [ ] 完整功能测试
- [ ] 性能测试
- [ ] 更新文档

**Day 5: 发布准备**
- [ ] 最终测试
- [ ] 创建发布版本
- [ ] 编写升级说明

---

## 详细修复代码示例

### 示例1: MotionPath构造函数

#### 修复前:
```cpp
MotionPath::MotionPath(const MObject &object)
{
    thisObject = object;

    MFnDependencyNode depNodFn(object);
    txPlug = depNodFn.findPlug("translateX");
    tyPlug = depNodFn.findPlug("translateY");
    tzPlug = depNodFn.findPlug("translateZ");

    rxPlug = depNodFn.findPlug("rotateX");
    ryPlug = depNodFn.findPlug("rotateY");
    rzPlug = depNodFn.findPlug("rotateZ");

    rpxPlug = depNodFn.findPlug("rotatePivotX");
    rpyPlug = depNodFn.findPlug("rotatePivotY");
    rpzPlug = depNodFn.findPlug("rotatePivotZ");

    rptxPlug = depNodFn.findPlug("rotatePivotTranslateX");
    rptyPlug = depNodFn.findPlug("rotatePivotTranslateY");
    rptzPlug = depNodFn.findPlug("rotatePivotTranslateZ");

    // ... 其余代码
}
```

#### 修复后:
```cpp
MotionPath::MotionPath(const MObject &object)
{
    thisObject = object;

    MFnDependencyNode depNodFn(object);
    MStatus status;

    // 平移属性
    txPlug = depNodFn.findPlug("translateX", false, &status);
    CHECK_MSTATUS(status);
    tyPlug = depNodFn.findPlug("translateY", false, &status);
    CHECK_MSTATUS(status);
    tzPlug = depNodFn.findPlug("translateZ", false, &status);
    CHECK_MSTATUS(status);

    // 旋转属性
    rxPlug = depNodFn.findPlug("rotateX", false, &status);
    CHECK_MSTATUS(status);
    ryPlug = depNodFn.findPlug("rotateY", false, &status);
    CHECK_MSTATUS(status);
    rzPlug = depNodFn.findPlug("rotateZ", false, &status);
    CHECK_MSTATUS(status);

    // Pivot属性
    rpxPlug = depNodFn.findPlug("rotatePivotX", false, &status);
    CHECK_MSTATUS(status);
    rpyPlug = depNodFn.findPlug("rotatePivotY", false, &status);
    CHECK_MSTATUS(status);
    rpzPlug = depNodFn.findPlug("rotatePivotZ", false, &status);
    CHECK_MSTATUS(status);

    // Pivot Translate属性
    rptxPlug = depNodFn.findPlug("rotatePivotTranslateX", false, &status);
    CHECK_MSTATUS(status);
    rptyPlug = depNodFn.findPlug("rotatePivotTranslateY", false, &status);
    CHECK_MSTATUS(status);
    rptzPlug = depNodFn.findPlug("rotatePivotTranslateZ", false, &status);
    CHECK_MSTATUS(status);

    // ... 其余代码保持不变
}
```

### 示例2: 修复三元运算符问题

#### 修复前:
```cpp
void MotionPath::expandKeyFramesCache(const MFnAnimCurve &curve,
                                       const Keyframe::Axis &axisName,
                                       bool isTranslate)
{
    int numKeys = curve.numKeys();

    double endTime = isDrawing ? endDrawingTime : displayEndTime;  // ❌ 宏冲突

    for(int i = 0; i < numKeys; i++)
    {
        // ...
    }
}
```

#### 修复后:
```cpp
void MotionPath::expandKeyFramesCache(MFnAnimCurve &curve,  // 移除const
                                       const Keyframe::Axis &axisName,
                                       bool isTranslate)
{
    int numKeys = curve.numKeys();

    // 使用括号保护或临时变量
    double endTime = (this->isDrawing) ? (this->endDrawingTime) : (this->displayEndTime);

    for(int i = 0; i < numKeys; i++)
    {
        // ...
    }
}
```

### 示例3: evaluateNumElements修复

#### 修复前:
```cpp
void MotionPath::findParentMatrixPlug(const MObject &transform,
                                       const bool constrained,
                                       MPlug &matrixPlug)
{
    MFnDagNode dagNodeFn(transform);
    MPlug parentMatrixPlugs = dagNodeFn.findPlug(constrained ? "worldMatrix" : "parentMatrix");
    parentMatrixPlugs.evaluateNumElements();  // ❌ 已弃用
    matrixPlug = parentMatrixPlugs[0];
}
```

#### 修复后:
```cpp
void MotionPath::findParentMatrixPlug(const MObject &transform,
                                       const bool constrained,
                                       MPlug &matrixPlug)
{
    MFnDagNode dagNodeFn(transform);
    MStatus status;

    const char* attrName = constrained ? "worldMatrix" : "parentMatrix";
    MPlug parentMatrixPlugs = dagNodeFn.findPlug(attrName, false, &status);
    CHECK_MSTATUS(status);

    // ✅ 使用新API
    matrixPlug = parentMatrixPlugs.elementByLogicalIndex(0, &status);
    CHECK_MSTATUS(status);
}
```

---

## 功能保留清单

### ✅ 100%保留的功能

#### 可视化
- [x] 运动路径曲线显示
- [x] 关键帧点显示
- [x] 切线显示
- [x] 帧编号显示
- [x] 当前帧高亮
- [x] 旋转关键帧显示
- [x] 世界空间模式
- [x] 相机空间模式
- [x] 颜色自定义
- [x] 大小自定义

#### 编辑
- [x] 移动关键帧
- [x] 编辑切线
- [x] 添加关键帧
- [x] 删除关键帧
- [x] 选择关键帧
- [x] 框选
- [x] 描边模式（Closest/Spread）

#### 缓冲路径
- [x] 创建缓冲路径
- [x] 删除缓冲路径
- [x] 选择缓冲路径
- [x] 转换为NURBS曲线

#### 高级功能
- [x] 复制/粘贴关键帧
- [x] 偏移粘贴
- [x] 锁定选择模式
- [x] Use Pivots模式
- [x] 约束检测
- [x] 动画层检测

#### 命令和脚本
- [x] tcMotionPathCmd所有标志
- [x] tcMotionPathEditContext
- [x] tcMotionPathDrawContext
- [x] Python集成
- [x] MEL集成

### ✅ 保留的内部逻辑

- [x] 关键帧缓存算法
- [x] 父矩阵缓存
- [x] Pivot矩阵计算
- [x] 切线计算
- [x] 世界空间转换
- [x] 相机空间转换
- [x] 选择管理
- [x] 撤销/重做系统
- [x] 回调机制
- [x] 设置持久化

---

## 风险评估

### 高风险
1. **const MFnAnimCurve**: 可能需要大量修改
2. **Windows宏冲突**: 影响多个文件
3. **Qt 6迁移**: 可能有隐藏的兼容性问题

### 中风险
1. **编译器差异**: VS2022 vs GCC
2. **Maya API变化**: 未文档化的变化
3. **性能回归**: 新API可能有性能差异

### 低风险
1. **OpenGL调用**: 已有VP2.0备份
2. **MEL脚本**: 通常向后兼容
3. **Python脚本**: Python 3兼容性较好

---

## 总结

### 需要修改的代码位置

| 文件 | 修改数量 | 类型 | 优先级 |
|------|---------|------|--------|
| MotionPath.cpp | 20+ | API更新 | 🔴 高 |
| CameraCache.cpp | 8+ | API更新 | 🔴 高 |
| 所有.cpp文件 | 17个 | 添加NOMINMAX | 🔴 高 |
| MotionPath.h | 3+ | 移除const | 🔴 高 |
| Makefile | 5+ | 编译选项 | 🔴 高 |

### 不需要修改的代码

✅ **DrawUtils.cpp** - OpenGL代码保留
✅ **Vp2DrawUtils.cpp** - 已经是现代API
✅ **MotionPathCmd.cpp** - 命令接口稳定
✅ **Keyframe.cpp** - 数据结构稳定
✅ **BufferPath.cpp** - 逻辑无需改变
✅ **GlobalSettings.cpp** - 配置系统稳定

### 预期成果

升级后的插件将：
- ✅ 在Maya 2025上成功编译
- ✅ 保留100%原始功能
- ✅ 支持VP2.0渲染（已有）
- ✅ 通过所有功能测试
- ✅ 保持向后兼容性（可选择支持旧版本）

---

**文档版本**: 1.0
**最后更新**: 2025-10-29
**作者**: Claude Code Analysis
