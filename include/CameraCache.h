//
//  CameraCache.h
//  MotionPath
//
//  Created by Daniele Federico on 11/05/15.
//
//

#ifndef MotionPath_CameraCache_h
#define MotionPath_CameraCache_h

#include <maya/MFloatMatrix.h>
#include <maya/MMatrix.h>
#include <maya/MFnCamera.h>
#include <maya/MDagPath.h>
#include <maya/MPlug.h>

#include <map>
#include <unordered_map>

class CameraCache
{
    public:
        CameraCache();
        ~CameraCache(){}

        // A2: Changed to unordered_map with int64_t tick keys for O(1) lookup
        std::unordered_map<int64_t, MMatrix> matrixCache;
        //std::map<double, MMatrix> projMatrixCache;
        int portWidth;
        int portHeight;

        // A2: Time to tick conversion (Maya uses 6000 ticks per time unit)
        static inline int64_t timeToTick(double time) {
            return static_cast<int64_t>(time * 6000.0 + 0.5);
        }
        static inline double tickToTime(int64_t tick) {
            return static_cast<double>(tick) / 6000.0;
        }
    
        bool isCaching(){return caching;}
        bool isInitialized(){return initialized;}
    
        void initialize(const MObject &camera);
    
        void cacheCamera();
        void ensureMatricesAtTime(const double time, const bool force=false);
        void checkRangeIsCached();
    
    private:
        bool caching, initialized;
        MPlug worldMatrixPlug;
        MPlug txPlug, tyPlug, tzPlug;
        MPlug rxPlug, ryPlug, rzPlug;
};

typedef std::map<std::string, CameraCache> CameraCacheMap;
typedef std::map<std::string, CameraCache>::iterator CameraCacheMapIterator;

#endif
