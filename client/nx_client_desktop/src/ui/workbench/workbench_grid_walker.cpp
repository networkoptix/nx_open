#include "workbench_grid_walker.h"


QnWorkbenchGridWalker::QnWorkbenchGridWalker():
    m_rect(0.0, 0.0, 1.0, 1.0),
    m_pos(0, 0),
    m_delta(0, 1)
{

}

QPoint QnWorkbenchGridWalker::next()
{
    NX_ASSERT(hasNext());

    QPoint result = m_pos;
    m_pos += m_delta;
    return result;
}

bool QnWorkbenchGridWalker::hasNext() const
{
    return m_rect.contains(m_pos);
}

void QnWorkbenchGridWalker::expand(Qt::Edge border)
{
    NX_ASSERT(!hasNext());

    switch (border)
    {
        case Qt::LeftEdge:
            m_rect.setLeft(m_rect.left() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(0, 1);
            break;
        case Qt::RightEdge:
            m_rect.setRight(m_rect.right() + 1);
            m_pos = m_rect.topRight();
            m_delta = QPoint(0, 1);
            break;
        case Qt::TopEdge:
            m_rect.setTop(m_rect.top() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(1, 0);
            break;
        case Qt::BottomEdge:
            m_rect.setBottom(m_rect.bottom() + 1);
            m_pos = m_rect.bottomLeft();
            m_delta = QPoint(1, 0);
            break;
        default:
            break;
    }
}

const QRect& QnWorkbenchGridWalker::rect() const
{
    return m_rect;
}
