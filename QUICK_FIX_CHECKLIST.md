# Maya Motion Path 升级到 Maya 2025 - 快速修复清单

## 📌 开始前确认

- [ ] 备份原始代码
- [ ] 安装Maya 2025
- [ ] 安装VS2022或GCC 11+
- [ ] 准备测试场景

---

## ⚡ 必须修复（让代码编译通过）

### 1. 创建PlatformFixes.h 【5分钟】

创建文件: `include/PlatformFixes.h`

```cpp
#ifndef PLATFORM_FIXES_H
#define PLATFORM_FIXES_H

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifdef min
        #undef min
    #endif
    #ifdef max
        #undef max
    #endif
#endif

#include <algorithm>

#define TC_FIND_PLUG(node, attrName, plug) \
    { MStatus _st; plug = node.findPlug(attrName, false, &_st); CHECK_MSTATUS(_st); }

#endif
```

---

### 2. 所有.cpp文件添加include 【30分钟】

**在每个.cpp文件的第一行添加**:
```cpp
#include "PlatformFixes.h"
```

**文件列表** (17个):
```
✅ source/MotionPath.cpp
✅ source/MotionPathManager.cpp
✅ source/MotionPathEditContext.cpp
✅ source/MotionPathDrawContext.cpp
✅ source/MotionPathCmd.cpp
✅ source/MotionPathOverride.cpp
✅ source/DrawUtils.cpp
✅ source/Vp2DrawUtils.cpp
✅ source/CameraCache.cpp
✅ source/BufferPath.cpp
✅ source/Keyframe.cpp
✅ source/KeyClipboard.cpp
✅ source/ContextUtils.cpp
✅ source/GlobalSettings.cpp
✅ source/animCurveUtils.cpp
✅ source/PluginMain.cpp
✅ source/MotionPathEditContextMenuWidget.cpp
```

---

### 3. 修复const MFnAnimCurve【30分钟】

#### 头文件修改

**include/MotionPath.h**:
```cpp
// 第164行 - 移除const
void expandKeyFramesCache(MFnAnimCurve &curve, const Keyframe::Axis &axisName, bool isTranslate);

// 第165行 - 移除所有6个const
void cacheKeyFrames(MFnAnimCurve &curveTX, MFnAnimCurve &curveTY, MFnAnimCurve &curveTZ,
                    MFnAnimCurve &curveRX, MFnAnimCurve &curveRY, MFnAnimCurve &curveRZ,
                    CameraCache* cachePtr, const MMatrix &currentCameraMatrix);

// 第166行 - 移除所有3个const
void setShowInOutTangents(MFnAnimCurve &curveTX, MFnAnimCurve &curveTY, MFnAnimCurve &curveTZ);
```

**include/Keyframe.h**:
```cpp
// 第35行 - 移除const
void setTangent(int keyIndex, MFnAnimCurve &curve, const Keyframe::Axis &axisName, const Keyframe::Tangent &tangentName);
```

#### 实现文件修改

**source/MotionPath.cpp**:
```cpp
// 第444行
void MotionPath::expandKeyFramesCache(MFnAnimCurve &curve, ...)  // 移除const

// 第517行
void MotionPath::setShowInOutTangents(MFnAnimCurve &curveTX, ...)  // 移除所有const

// 第611行
void MotionPath::cacheKeyFrames(MFnAnimCurve &curveTX, ...)  // 移除所有const

// 内部函数（找到并修改）:
double getTangentValueForClipboard(MFnAnimCurve &curve, ...)  // 移除const
MVector evaluateTangentForClipboard(MFnAnimCurve &cx, ...)  // 第一个移除const
bool isCurveBoundaryKey(MFnAnimCurve &curve, ...)  // 移除const
void restoreTangents(MFnAnimCurve &fnSource, ...)  // 第一个移除const
bool breakTangentsForKeyCopy(MFnAnimCurve &curve, ...)  // 移除const
```

**source/Keyframe.cpp**:
```cpp
// 第58行
void Keyframe::setTangent(int keyIndex, MFnAnimCurve &curve, ...)  // 移除const
```

---

### 4. 修复findPlug【1-2小时】

#### source/MotionPath.cpp

```cpp
// 第33-47行 - 构造函数中
MStatus status;
txPlug = depNodFn.findPlug("translateX", false, &status);
tyPlug = depNodFn.findPlug("translateY", false, &status);
tzPlug = depNodFn.findPlug("translateZ", false, &status);
rxPlug = depNodFn.findPlug("rotateX", false, &status);
ryPlug = depNodFn.findPlug("rotateY", false, &status);
rzPlug = depNodFn.findPlug("rotateZ", false, &status);
rpxPlug = depNodFn.findPlug("rotatePivotX", false, &status);
rpyPlug = depNodFn.findPlug("rotatePivotY", false, &status);
rpzPlug = depNodFn.findPlug("rotatePivotZ", false, &status);
rptxPlug = depNodFn.findPlug("rotatePivotTranslateX", false, &status);
rptyPlug = depNodFn.findPlug("rotatePivotTranslateY", false, &status);
rptzPlug = depNodFn.findPlug("rotatePivotTranslateZ", false, &status);

// 第109-115行 - cacheParentMatrixRangeForWorldCallback
MPlug txP = depNodFn.findPlug("translateX", false, &status);
MPlug tyP = depNodFn.findPlug("translateY", false, &status);
MPlug tzP = depNodFn.findPlug("translateZ", false, &status);
MPlug rxP = depNodFn.findPlug("rotateX", false, &status);
MPlug ryP = depNodFn.findPlug("rotateY", false, &status);
MPlug rzP = depNodFn.findPlug("rotateZ", false, &status);

// 第225-227行 - worldMatrixChangedCallback
MPlug txPlug = depNodFn.findPlug("translateX", false, &status);
MPlug tyPlug = depNodFn.findPlug("translateY", false, &status);
MPlug tzPlug = depNodFn.findPlug("translateZ", false, &status);

// 第307行 - findParentMatrixPlug
MPlug parentMatrixPlugs = dagNodeFn.findPlug(constrained ? "worldMatrix" : "parentMatrix", false, &status);
```

#### source/CameraCache.cpp

```cpp
// 第29行
MPlug worldMatrixPlugs = fnCamera.findPlug("worldMatrix", false, &status);

// 第41-47行
txPlug = transformFn.findPlug("translateX", false, &status);
tyPlug = transformFn.findPlug("translateY", false, &status);
tzPlug = transformFn.findPlug("translateZ", false, &status);
rxPlug = transformFn.findPlug("rotateX", false, &status);
ryPlug = transformFn.findPlug("rotateY", false, &status);
rzPlug = transformFn.findPlug("rotateZ", false, &status);
```

#### source/MotionPathManager.cpp

```cpp
// 第494行
MStatus status;
if (!dependNodeFn.findPlug("translate", false, &status).isNull())
```

---

### 5. 修复evaluateNumElements【10分钟】

#### source/MotionPath.cpp:307-309

```cpp
// 修复前
MPlug parentMatrixPlugs = dagNodeFn.findPlug(constrained ? "worldMatrix" : "parentMatrix");
parentMatrixPlugs.evaluateNumElements();
matrixPlug = parentMatrixPlugs[0];

// 修复后
MStatus status;
const char* attrName = constrained ? "worldMatrix" : "parentMatrix";
MPlug parentMatrixPlugs = dagNodeFn.findPlug(attrName, false, &status);
matrixPlug = parentMatrixPlugs.elementByLogicalIndex(0, &status);
```

#### source/CameraCache.cpp:29-31

```cpp
// 修复前
MPlug worldMatrixPlugs = fnCamera.findPlug("worldMatrix");
worldMatrixPlugs.evaluateNumElements();
worldMatrixPlug = worldMatrixPlugs[0];

// 修复后
MStatus status;
MPlug worldMatrixPlugs = fnCamera.findPlug("worldMatrix", false, &status);
worldMatrixPlug = worldMatrixPlugs.elementByLogicalIndex(0, &status);
```

---

### 6. 修复MotionPath.cpp:448【5分钟】

```cpp
// 修复前
double endTime = isDrawing ? endDrawingTime : displayEndTime;

// 修复后
double endTime = (this->isDrawing) ? (this->endDrawingTime) : (this->displayEndTime);
```

---

### 7. 更新Makefile【10分钟】

```makefile
# 修改前
C++ = g++412
MAYAVERSION = 2015
CFLAGS = -m64 -pthread -pipe -D_BOOL -DLINUX -DREQUIRE_IOSTREAM

# 修改后
C++ = g++
MAYAVERSION = 2025
CFLAGS = -std=c++17 -m64 -pthread -pipe -D_BOOL -DLINUX -DREQUIRE_IOSTREAM -DNOMINMAX

MAYA_LOCATION = /usr/autodesk/maya$(MAYAVERSION)
```

---

## 🏁 编译和测试

### 编译

```bash
# Linux
make clean
make all

# Windows
nmake clean
nmake all
```

### 加载测试

```mel
// Maya中
loadPlugin "tcMotionPath";
```

应该看到:
```
// Result: tcMotionPath //
```

### 功能测试

```mel
// 激活插件
tcMotionPathCmd -enable true;

// 创建测试对象
sphere;
setKeyframe -t 1 -v 0 0 0 pSphere1.translate;
setKeyframe -t 24 -v 10 5 0 pSphere1.translate;
setKeyframe -t 48 -v 0 0 0 pSphere1.translate;

// 选择对象
select pSphere1;

// 应该看到运动路径
```

---

## 📊 进度跟踪

### 修复进度

- [ ] 1. 创建PlatformFixes.h
- [ ] 2. 添加include到所有文件
- [ ] 3. 修复const MFnAnimCurve
  - [ ] include/MotionPath.h (3处)
  - [ ] include/Keyframe.h (1处)
  - [ ] source/MotionPath.cpp (9处)
  - [ ] source/Keyframe.cpp (1处)
- [ ] 4. 修复findPlug
  - [ ] source/MotionPath.cpp (20处)
  - [ ] source/CameraCache.cpp (8处)
  - [ ] source/MotionPathManager.cpp (1处)
- [ ] 5. 修复evaluateNumElements (2处)
- [ ] 6. 修复三元运算符 (1处)
- [ ] 7. 更新Makefile

### 测试进度

- [ ] 编译成功
- [ ] 加载插件成功
- [ ] 基础可视化测试
- [ ] 编辑功能测试
- [ ] 缓冲路径测试
- [ ] 复制粘贴测试
- [ ] 描边模式测试

---

## 🚨 常见问题

### Q: 编译时仍然有C2568错误？
A: 确保PlatformFixes.h是第一个include，在所有其他头文件之前。

### Q: 链接错误？
A: 确保Makefile中的MAYA_LOCATION正确，指向Maya 2025安装目录。

### Q: 插件加载失败？
A: 检查Maya版本匹配，确保编译时使用了Maya 2025的头文件和库。

### Q: 功能不正常？
A: 对比详细文档中的原始代码逻辑，确保只修改了API调用，没有改变业务逻辑。

---

## 📚 参考文档

详细信息请查看：
- `UPGRADE_ANALYSIS_DETAILED.md` - 完整技术分析
- `VS2022_COMPILATION_FIXES.md` - VS2022编译错误详解

---

## ⏱️ 预计时间

| 任务 | 时间 |
|------|------|
| 创建PlatformFixes.h | 5分钟 |
| 添加include | 30分钟 |
| 修复const MFnAnimCurve | 30分钟 |
| 修复findPlug | 1-2小时 |
| 修复evaluateNumElements | 10分钟 |
| 修复三元运算符 | 5分钟 |
| 更新Makefile | 10分钟 |
| **总计** | **3-4小时** |
| 编译和测试 | 1-2小时 |
| **总工作量** | **4-6小时** |

---

## ✅ 完成标准

代码升级成功的标准：
- [x] 编译无错误
- [x] 编译无警告（或只有无关紧要的警告）
- [x] 插件在Maya 2025中成功加载
- [x] 所有原始功能正常工作
- [x] 没有crash或异常
- [x] 性能无明显下降

---

**祝您升级顺利！** 🚀

如有问题，请参考详细文档或搜索Maya 2025 API文档。
