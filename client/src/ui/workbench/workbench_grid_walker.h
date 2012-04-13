#ifndef QN_WORKBENCH_GRID_WALKER_H
#define QN_WORKBENCH_GRID_WALKER_H

#include <QRect>
#include <QPoint>
#include "workbench_globals.h"

/**
 * This is a helper class for guided iteration over workbench layout grid.
 */
class QnWorkbenchGridWalker {
public:
    /**
     * Constructor. 
     */
    QnWorkbenchGridWalker() {
        m_rect = QRect(0, 0, 1, 1);
        m_pos = QPoint(0, 0);
        m_delta = QPoint(0, 1);
    }

    /**
     * Returns current position and moves to the next one. Next position must exist.
     * 
     * \returns                         Current position.
     */
    QPoint next() {
        Q_ASSERT(hasNext());

        QPoint result = m_pos;
        m_pos += m_delta;
        return result;
    }

    /**
     * Use this function to check whether there are more position that were not
     * iterated over yet. In case there are none, <tt>expand</tt> function
     * should be called to expand the iteration region.
     *
     * \returns                         Whether the next position exists. 
     */
    bool hasNext() const {
        return m_rect.contains(m_pos);
    }

    /**
     * Expands the iteration region in the direction of the given border.
     * Next position must not exist.
     * 
     * \param border                    Border to expand iteration region into.
     */
    void expand(Qn::Border border) {
        Q_ASSERT(!hasNext());

        switch(border) {
        case Qn::LeftBorder:
            m_rect.setLeft(m_rect.left() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(0, 1);
            break;
        case Qn::RightBorder:
            m_rect.setRight(m_rect.right() + 1);
            m_pos = m_rect.topRight();
            m_delta = QPoint(0, 1);
            break;
        case Qn::TopBorder:
            m_rect.setTop(m_rect.top() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(1, 0);
            break;
        case Qn::BottomBorder:
            m_rect.setBottom(m_rect.bottom() + 1);
            m_pos = m_rect.bottomLeft();
            m_delta = QPoint(1, 0);
            break;
        default:
            break;
        }
    }

    /**
     * \returns                         Current iteration region.
     */
    const QRect &rect() const {
        return m_rect;
    }

private:
    QRect m_rect;
    QPoint m_pos;
    QPoint m_delta;
};

#endif // QN_WORKBENCH_GRID_WALKER_H
