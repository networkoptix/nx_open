#ifndef QN_WORKBENCH_GRID_WALKER_H
#define QN_WORKBENCH_GRID_WALKER_H

#include <QRect>
#include <QPoint>

/**
 * This is a helper class for guided iteration over workbench layout grid.
 */
class QnWorkbenchGridWalker {
public:
    enum Border {
        NoBorders = 0,
        LeftBorder = 0x1,
        RightBorder = 0x2,
        TopBorder = 0x4,
        BottomBorder = 0x8,
        AllBorders = LeftBorder | RightBorder | TopBorder | BottomBorder
    };
    Q_DECLARE_FLAGS(Borders, Border)

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
    void expand(Border border) {
        Q_ASSERT(!hasNext());

        switch(border) {
        case LeftBorder:
            m_rect.setLeft(m_rect.left() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(0, 1);
            break;
        case RightBorder:
            m_rect.setRight(m_rect.right() + 1);
            m_pos = m_rect.topRight();
            m_delta = QPoint(0, 1);
            break;
        case TopBorder:
            m_rect.setTop(m_rect.top() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(1, 0);
            break;
        case BottomBorder:
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

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchGridWalker::Borders)

#endif // QN_WORKBENCH_GRID_WALKER_H
