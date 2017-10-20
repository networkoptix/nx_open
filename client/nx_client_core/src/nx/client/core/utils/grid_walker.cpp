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
            const bool goingRight = ((y & 1) == 0); //< Odd row.
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
            const int xR = grid.width() - x - 1; //< X coordinate, counting from right.
            const int yB = grid.height() - y - 1; //< Y coordinate, counting from bottom.
            const QPoint mid((grid.width() + 1) / 2, (grid.height() + 1) / 2);

            // Top sector of crossing of the lines (x + 1 == y) and (xR == y) above the center.
            const bool goingRight = (x + 1 >= y) && (xR > y) && (y < mid.y());

            // Right sector of crossing of the lines (xR == y) and (xR == yB) right of center.
            const bool goingDown = (y >= xR) && (yB > xR) && (x >= mid.x());

            // Bottom sector of crossing of the lines (xR == yB) and (x == yB) below the center
            const bool goingLeft = (x > yB) && (xR >= yB) && (y >= mid.y());

            // Left sector of crossing of the lines (x + 1 == y) and (x == yB) left of center.
            const bool goingUp = (x <= yB) && (y > x + 1) && (x < mid.x());

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
