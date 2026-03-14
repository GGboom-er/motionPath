//
//  PickingGrid.h
//  MotionPath
//
//  B1 Optimization: Spatial acceleration grid for picking
//  Reduces O(N) linear search to O(1) grid lookup
//

#ifndef PICKINGGRID_H
#define PICKINGGRID_H

#include <vector>
#include <unordered_map>
#include <cstdint>

// B1: Spatial hash grid for fast picking
// Divides screen space into grid cells for O(1) lookup
class PickingGrid
{
public:
    // Hit types stored in grid
    enum HitType {
        HIT_KEYFRAME,
        HIT_TANGENT_IN,
        HIT_TANGENT_OUT,
        HIT_FRAME
    };

    // Single hit record
    struct Hit {
        HitType type;
        int motionPathIndex;  // Index in MotionPathManager
        int keyframeId;       // For keyframes/tangents
        double time;          // For frames
        double screenX;       // Cached screen position
        double screenY;

        Hit(HitType t, int mpIdx, int kfId, double tm, double x, double y)
            : type(t), motionPathIndex(mpIdx), keyframeId(kfId), time(tm), screenX(x), screenY(y) {}
    };

    PickingGrid(int viewportWidth, int viewportHeight, int cellSize = 32);
    ~PickingGrid();

    // Clear and rebuild grid
    void clear();

    // Add hits to grid
    void addKeyframe(int mpIndex, int keyframeId, double screenX, double screenY);
    void addTangentIn(int mpIndex, int keyframeId, double screenX, double screenY);
    void addTangentOut(int mpIndex, int keyframeId, double screenX, double screenY);
    void addFrame(int mpIndex, double time, double screenX, double screenY);

    // Query grid at mouse position
    // Returns all hits within search radius (sorted by distance)
    std::vector<Hit> query(double mouseX, double mouseY, double searchRadius) const;

    // Public accessors for viewport dimensions (used by MotionPathManager)
    int getViewportWidth() const { return m_viewportWidth; }
    int getViewportHeight() const { return m_viewportHeight; }

private:
    int m_viewportWidth;
    int m_viewportHeight;
    int m_cellSize;
    int m_gridWidth;   // Number of cells in X
    int m_gridHeight;  // Number of cells in Y

    // Grid storage: cell index -> list of hits
    std::unordered_map<int64_t, std::vector<Hit>> m_grid;

    // Convert screen coordinates to cell index
    inline int64_t getCellIndex(double x, double y) const {
        int cellX = static_cast<int>(x / m_cellSize);
        int cellY = static_cast<int>(y / m_cellSize);

        // Clamp to grid bounds
        if (cellX < 0) cellX = 0;
        if (cellY < 0) cellY = 0;
        if (cellX >= m_gridWidth) cellX = m_gridWidth - 1;
        if (cellY >= m_gridHeight) cellY = m_gridHeight - 1;

        return static_cast<int64_t>(cellY) * m_gridWidth + cellX;
    }

    // Get neighboring cells for query
    void getNeighborCells(int64_t centerCell, std::vector<int64_t> &neighbors) const;
};

#endif // PICKINGGRID_H
