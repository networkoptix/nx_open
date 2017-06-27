#pragma once

#include <QtCore/QRect>
#include <QtCore/QPoint>

/**
 * This is a helper class for guided iteration over workbench layout grid.
 */
class QnWorkbenchGridWalker
{
public:
    /**
     * Constructor.
     */
    QnWorkbenchGridWalker();

    /**
     * Returns current position and moves to the next one. Next position must exist.
     *
     * \returns                         Current position.
     */
    QPoint next();

    /**
     * Use this function to check whether there are more position that were not
     * iterated over yet. In case there are none, <tt>expand</tt> function
     * should be called to expand the iteration region.
     *
     * \returns                         Whether the next position exists.
     */
    bool hasNext() const;

    /**
     * Expands the iteration region in the direction of the given border.
     * Next position must not exist.
     *
     * \param border                    Border to expand iteration region into.
     */
    void expand(Qt::Edge border);

    /**
     * \returns                         Current iteration region.
     */
    const QRect& rect() const;

private:
    QRect m_rect;
    QPoint m_pos;
    QPoint m_delta;
};
