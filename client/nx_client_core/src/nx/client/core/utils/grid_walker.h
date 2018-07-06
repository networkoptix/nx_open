#pragma once

#include <QtCore/QRect>

namespace nx {
namespace client {
namespace core {

/**
 * Walk around the given grid, starting from the top left corner. Default state is invalid until
 * next() is called.
 */
struct GridWalker
{
    enum class Policy
    {
        Sequential,
        Snake,
        Round,
    };

    GridWalker(const QRect& grid, Policy policy = Policy::Sequential);

    QPoint pos() const;

    /**
     * Go to the next cell.
     * @return true if current state is valid.
     */
    bool next();

    int x;
    int y;
    const QRect grid;
    const Policy policy;
};

} // namespace core
} // namespace client
} // namespace nx
