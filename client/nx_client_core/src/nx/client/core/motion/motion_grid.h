#pragma once

#include <array>

#include <motion/motion_detection.h>

class QPoint;
class QRect;

namespace nx {
namespace client {
namespace core {

class MotionGrid
{
public:
    static constexpr int kWidth = Qn::kMotionGridWidth;
    static constexpr int kHeight = Qn::kMotionGridHeight;
    static constexpr int kCellCount = kWidth * kHeight;

public:
    // Item access.
    int operator[](const QPoint& pos) const;
    int& operator[](const QPoint& pos);

    // Fill grid with zeros.
    void reset();

    // Fill specified rectangle with specified value.
    void fillRect(const QRect& rect, int value);

    // Fill consecutive region containing specified point with specified value.
    void fillRegion(const QPoint& at, int value);

    // Fill consecutive region containing specified point with zeros.
    void clearRegion(const QPoint& at);

private:
    using Row = std::array<int, kWidth>;
    using Grid = std::array<Row, kHeight>;
    Grid m_grid = Grid();
};

} // namespace core
} // namespace client
} // namespace nx
