//
//  PickingGrid.cpp
//  MotionPath
//
//  B1 Optimization: Spatial acceleration grid implementation
//

#include "PickingGrid.h"
#include <algorithm>
#include <cmath>

PickingGrid::PickingGrid(int viewportWidth, int viewportHeight, int cellSize)
    : m_viewportWidth(viewportWidth)
    , m_viewportHeight(viewportHeight)
    , m_cellSize(cellSize)
{
    m_gridWidth = (viewportWidth + cellSize - 1) / cellSize;
    m_gridHeight = (viewportHeight + cellSize - 1) / cellSize;

    // Pre-allocate some capacity to reduce allocations
    m_grid.reserve(m_gridWidth * m_gridHeight / 4);  // Assume 25% occupancy
}

PickingGrid::~PickingGrid()
{
    clear();
}

void PickingGrid::clear()
{
    m_grid.clear();
}

void PickingGrid::addKeyframe(int mpIndex, int keyframeId, double screenX, double screenY)
{
    int64_t cell = getCellIndex(screenX, screenY);
    m_grid[cell].emplace_back(HIT_KEYFRAME, mpIndex, keyframeId, 0.0, screenX, screenY);
}

void PickingGrid::addTangentIn(int mpIndex, int keyframeId, double screenX, double screenY)
{
    int64_t cell = getCellIndex(screenX, screenY);
    m_grid[cell].emplace_back(HIT_TANGENT_IN, mpIndex, keyframeId, 0.0, screenX, screenY);
}

void PickingGrid::addTangentOut(int mpIndex, int keyframeId, double screenX, double screenY)
{
    int64_t cell = getCellIndex(screenX, screenY);
    m_grid[cell].emplace_back(HIT_TANGENT_OUT, mpIndex, keyframeId, 0.0, screenX, screenY);
}

void PickingGrid::addFrame(int mpIndex, double time, double screenX, double screenY)
{
    int64_t cell = getCellIndex(screenX, screenY);
    m_grid[cell].emplace_back(HIT_FRAME, mpIndex, -1, time, screenX, screenY);
}

std::vector<PickingGrid::Hit> PickingGrid::query(double mouseX, double mouseY, double searchRadius) const
{
    std::vector<Hit> results;
    double radiusSq = searchRadius * searchRadius;

    // Get center cell and neighbors
    int64_t centerCell = getCellIndex(mouseX, mouseY);
    std::vector<int64_t> cellsToCheck;
    cellsToCheck.push_back(centerCell);
    getNeighborCells(centerCell, cellsToCheck);

    // Check all hits in nearby cells
    for (int64_t cellIdx : cellsToCheck)
    {
        auto it = m_grid.find(cellIdx);
        if (it == m_grid.end())
            continue;

        const std::vector<Hit> &hits = it->second;
        for (const Hit &hit : hits)
        {
            double dx = mouseX - hit.screenX;
            double dy = mouseY - hit.screenY;
            double distSq = dx * dx + dy * dy;

            if (distSq <= radiusSq)
            {
                results.push_back(hit);
            }
        }
    }

    // Sort by distance (closest first)
    std::sort(results.begin(), results.end(), [mouseX, mouseY](const Hit &a, const Hit &b) {
        double distA = (mouseX - a.screenX) * (mouseX - a.screenX) + (mouseY - a.screenY) * (mouseY - a.screenY);
        double distB = (mouseX - b.screenX) * (mouseX - b.screenX) + (mouseY - b.screenY) * (mouseY - b.screenY);
        return distA < distB;
    });

    return results;
}

void PickingGrid::getNeighborCells(int64_t centerCell, std::vector<int64_t> &neighbors) const
{
    int centerX = static_cast<int>(centerCell % m_gridWidth);
    int centerY = static_cast<int>(centerCell / m_gridWidth);

    // Check 3x3 neighborhood (8 neighbors)
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
                continue;  // Skip center (already added)

            int nx = centerX + dx;
            int ny = centerY + dy;

            // Bounds check
            if (nx < 0 || nx >= m_gridWidth || ny < 0 || ny >= m_gridHeight)
                continue;

            int64_t neighborCell = static_cast<int64_t>(ny) * m_gridWidth + nx;
            neighbors.push_back(neighborCell);
        }
    }
}
