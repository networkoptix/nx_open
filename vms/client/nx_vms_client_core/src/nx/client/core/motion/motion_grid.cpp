#include "motion_grid.h"

#include <QtCore/QPoint>
#include <QtCore/QRect>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

int MotionGrid::operator[](const QPoint& pos) const
{
    return m_grid[pos.y()][pos.x()];
}

int& MotionGrid::operator[](const QPoint& pos)
{
    return m_grid[pos.y()][pos.x()];
}

void MotionGrid::reset()
{
    for (auto& row: m_grid)
        row.fill(0);
}

void MotionGrid::fillRect(const QRect& rect, int value)
{
    if (!rect.isValid())
        return;

    NX_ASSERT(rect.top() >= 0 && rect.bottom() < kHeight, Q_FUNC_INFO);
    NX_ASSERT(rect.left() >= 0 && rect.right() < kWidth, Q_FUNC_INFO);

    for (int row = rect.top(); row <= rect.bottom(); ++row)
        std::fill(m_grid[row].begin() + rect.left(), m_grid[row].begin() + rect.right() + 1, value);
}

void MotionGrid::fillRegion(const QPoint& at, int value)
{
    NX_ASSERT(at.y() >= 0 && at.y() < kHeight, Q_FUNC_INFO);
    NX_ASSERT(at.x() >= 0 && at.x() < kWidth, Q_FUNC_INFO);

    int regionValue = m_grid[at.y()][at.x()];
    if (regionValue == value)
        return; //< Nothing to do.

    QVarLengthArray<QPoint> pointStack;
    m_grid[at.y()][at.x()] = value;
    pointStack.push_back(at);

    while (!pointStack.empty())
    {
        QPoint p = pointStack.back();
        pointStack.pop_back();

        // Spread left.
        int x = p.x() - 1;
        if (x >= 0 && m_grid[p.y()][x] == regionValue)
        {
            m_grid[p.y()][x] = value;
            pointStack.push_back({x, p.y()});
        }

        // Spread right.
        x = p.x() + 1;
        if (x < kWidth && m_grid[p.y()][x] == regionValue)
        {
            m_grid[p.y()][x] = value;
            pointStack.push_back({x, p.y()});
        }

        // Spread up.
        int y = p.y() - 1;
        if (y >= 0 && m_grid[y][p.x()] == regionValue)
        {
            m_grid[y][p.x()] = value;
            pointStack.push_back({p.x(), y});
        }

        // Spread down.
        y = p.y() + 1;
        if (y < kHeight && m_grid[y][p.x()] == regionValue)
        {
            m_grid[y][p.x()] = value;
            pointStack.push_back({p.x(), y});
        }
    }
}

void MotionGrid::clearRegion(const QPoint& at)
{
    fillRegion(at, 0);
}

bool MotionGrid::operator==(const MotionGrid& other) const
{
    return m_grid == other.m_grid;
}

bool MotionGrid::operator!=(const MotionGrid& other) const
{
    return m_grid != other.m_grid;
}

} // namespace nx::vms::client::core
