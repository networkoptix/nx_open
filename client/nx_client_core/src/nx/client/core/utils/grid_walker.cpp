#include "grid_walker.h"
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace core {

GridWalker::GridWalker(const QRect& grid, Policy policy):
    x(grid.left() - 1),
    y(grid.top()),
    grid(grid),
    policy(policy)
{

}

QPoint GridWalker::pos() const
{
    return {x, y};
}

bool GridWalker::next()
{
    switch (policy)
    {
        case Policy::Sequential:
        {
            ++x;
            if (x > grid.right())
            {
                x = grid.left();
                ++y;
            }
            return y <= grid.bottom();
        }

        case Policy::Snake:
        {
            const bool goingRight = (((grid.top() + y) & 1) == 0); //< Odd row.
            if (goingRight)
            {
                if (x == grid.right())
                    ++y;
                else
                    ++x;
            }
            else
            {
                if (x == grid.left())
                    ++y;
                else
                    --x;
            }
            return y <= grid.bottom();
        }

        case Policy::Round:
        {
            const int xL = x - grid.left(); //< X coordinate, counting from left.
            const int yT = y - grid.top(); //< Y coordinate, counting from top.
            const int xR = grid.right() - x; //< X coordinate, counting from right.
            const int yB = grid.bottom() - y; //< Y coordinate, counting from bottom.
            const QPoint mid((grid.width() + 1) / 2, (grid.height() + 1) / 2);

            // Top sector of crossing of the lines (xL + 1 == yT) and (xR == yT) above the center.
            const bool goingRight = (xL + 1 >= yT) && (xR > yT) && (yT < mid.y());

            // Right sector of crossing of the lines (xR == yT) and (xR == yB) right of center.
            const bool goingDown = (yT >= xR) && (yB > xR) && (xL >= mid.x());

            // Bottom sector of crossing of the lines (xR == yB) and (xL == yB) below the center
            const bool goingLeft = (xL > yB) && (xR >= yB) && (yT >= mid.y());

            // Left sector of crossing of the lines (xL + 1 == yT) and (xL == yB) left of center.
            const bool goingUp = (xL <= yB) && (yT > xL + 1) && (xL < mid.x());

            if (goingRight)
            {
                NX_ASSERT(!goingDown && !goingLeft && !goingUp);
                ++x;
                return true;
            }
            if (goingDown)
            {
                NX_ASSERT(!goingLeft && !goingUp);
                ++y;
                return true;
            }
            if (goingLeft)
            {
                NX_ASSERT(!goingUp);
                --x;
                return true;
            }
            if (goingUp)
            {
                --y;
                return true;
            }
            return false;
        }
        default:
            break;
    }

    return false;
}

} // namespace core
} // namespace client
} // namespace nx
