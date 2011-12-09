#include "grid_walker.h"
#include <cmath> /* For std::abs. */

QnAspectRatioGridWalker::QnAspectRatioGridWalker(qreal aspectRatio): 
    m_aspectRatio(aspectRatio)
{
    reset();
}

QPoint QnAspectRatioGridWalker::next() {
    QPoint result = m_pos;
    advance();
    return result;
}

void QnAspectRatioGridWalker::advance() {
    m_pos += m_delta;
    if(m_rect.contains(m_pos))
        return;

    /* Need to grow. Find growth direction. */
    qreal horizontalArDelta = std::abs(std::log((static_cast<qreal>(m_rect.width() + 1) / m_rect.height()) / m_aspectRatio));
    qreal verticalArDelta   = std::abs(std::log((static_cast<qreal>(m_rect.width()) / (m_rect.height() + 1)) / m_aspectRatio));

    if(qFuzzyCompare(horizontalArDelta, verticalArDelta) || horizontalArDelta < verticalArDelta) {
        if(-m_rect.left() <= m_rect.right()) {
            m_rect.setLeft(m_rect.left() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(0, 1);
        } else {
            m_rect.setRight(m_rect.right() + 1);
            m_pos = m_rect.topRight();
            m_delta = QPoint(0, 1);
        }
    } else {
        if(-m_rect.top() <= m_rect.bottom()) {
            m_rect.setTop(m_rect.top() - 1);
            m_pos = m_rect.topLeft();
            m_delta = QPoint(1, 0);
        } else {
            m_rect.setBottom(m_rect.bottom() + 1);
            m_pos = m_rect.bottomLeft();
            m_delta = QPoint(1, 0);
        }
    }
}

void QnAspectRatioGridWalker::reset() {
    m_rect = QRect(0, 0, 1, 1);
    m_pos = QPoint(0, 0);
    m_delta = QPoint(0, 1);
}









