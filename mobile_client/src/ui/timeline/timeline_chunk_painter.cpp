#include "timeline_chunk_painter.h"

#include <array>

namespace {
    /** Minimal color coefficient for the most noticeable chunk color in range */
    const qreal lineBarMinNoticeableFraction = 0.5;

    QColor linearCombine(qreal a, const QColor &x, qreal b, const QColor &y) {
        return QColor(x.red() * a + y.red() * b,
                      x.green() * a + y.green() * b,
                      x.blue() * a + y.blue() * b,
                      x.alpha() * a + y.alpha() * b);
    }
}

std::array<QColor, Qn::TimePeriodContentCount + 1> QnTimelineChunkPainter::colors() const {
    return m_color;
}

void QnTimelineChunkPainter::setColors(const std::array<QColor, Qn::TimePeriodContentCount + 1> &colors) {
    m_color = colors;
}

void QnTimelineChunkPainter::start(qint64 startPos, qint64 centralPos, const QRectF &rect, int chunkCount, qint64 windowStart, qint64 windowEnd) {
    m_position = startPos;
    m_rect = rect.toRect();
    m_windowStart = windowStart;
    m_windowEnd = windowEnd;
    m_centralPosition = centralPos;
    m_centralCoordinate = xFromValue(m_centralPosition);
    m_minChunkLength = (windowEnd - windowStart) / rect.width();
    m_geometry->allocate((chunkCount + 2) * 6);
    m_points = m_geometry->vertexDataAsColoredPoint2D();
    m_index = 0;

    qFill(m_weights, 0);
}

void QnTimelineChunkPainter::paintChunk(qint64 length, Qn::TimePeriodContent content) {
    Q_ASSERT(length >= 0);

    if (m_index >= m_geometry->vertexCount() - 4)
        return;

    if (m_pendingLength > 0 && m_pendingLength + length > m_minChunkLength) {
        qint64 delta = m_minChunkLength - m_pendingLength;
        length -= delta;

        storeChunk(delta, content);
        flushChunk();
    }

    storeChunk(length, content);
    if (m_pendingLength >= m_minChunkLength)
        flushChunk();
}

void QnTimelineChunkPainter::stop() {
    if (m_pendingLength > 0)
        flushChunk();
    else if (m_index == 0)
        addRect(m_rect, m_color[Qn::TimePeriodContentCount]);

    while (m_index < m_geometry->vertexCount())
        m_points[m_index++].set(0, 0, 0, 0, 0, 0);
}

qreal QnTimelineChunkPainter::xFromValue(qint64 value) {
    if (m_windowStart == m_windowEnd)
        return 0;
    return m_rect.x() + m_rect.width() * (value - m_windowStart) / (m_windowEnd - m_windowStart);
}

void QnTimelineChunkPainter::storeChunk(qint64 length, Qn::TimePeriodContent content) {
    if (m_pendingLength == 0)
        m_pendingPosition = m_position;

    m_weights[content] += length;
    m_pendingLength += length;
    m_position += length;
}

void QnTimelineChunkPainter::flushChunk() {
    qint64 leftPosition = m_pendingPosition;
    qint64 rightPosition = m_pendingPosition + m_pendingLength;

    if (rightPosition <= m_windowStart || leftPosition > m_windowEnd)
        return;

    leftPosition = qMax(leftPosition, m_windowStart);
    rightPosition = qMin(rightPosition, m_windowEnd);

    qreal l = xFromValue(leftPosition);
    qreal r = xFromValue(rightPosition);

    addRect(QRectF(l, m_rect.top(), r - l, m_rect.height()), currentColor(m_color));

    m_pendingPosition = rightPosition;
    m_pendingLength = 0;

    qFill(m_weights, 0);
}

void QnTimelineChunkPainter::addRect(const QRectF &rect, const QColor &color) {
    m_points[m_index + 0].set(rect.left(),  rect.top(),     color.red(), color.green(), color.blue(), color.alpha());
    m_points[m_index + 1].set(rect.right(), rect.top(),     color.red(), color.green(), color.blue(), color.alpha());
    m_points[m_index + 2].set(rect.right(), rect.bottom(),  color.red(), color.green(), color.blue(), color.alpha());
    m_points[m_index + 3] = m_points[m_index + 0];
    m_points[m_index + 4] = m_points[m_index + 2];
    m_points[m_index + 5].set(rect.left(),  rect.bottom(),  color.red(), color.green(), color.blue(), color.alpha());
    m_index += 6;
}

QColor QnTimelineChunkPainter::currentColor(const std::array<QColor, Qn::TimePeriodContentCount + 1> &colors) const {
    qreal rc = m_weights[Qn::RecordingContent];
    qreal mc = m_weights[Qn::MotionContent];
    qreal bc = m_weights[Qn::BookmarksContent];
    qreal nc = m_weights[Qn::TimePeriodContentCount];
    qreal sum = m_pendingLength;

    if (!qFuzzyIsNull(bc) && !qFuzzyIsNull(mc)) {
        qreal localSum = mc + bc;
        return linearCombine(mc / localSum, colors[Qn::MotionContent], bc / localSum, colors[Qn::BookmarksContent]);
    }

    if (!qFuzzyIsNull(bc)) {
        /* Make sure bookmark is noticeable even if there isn't much of it.
             * Note that these adjustments don't change sum. */
        rc = rc * (1.0 - lineBarMinNoticeableFraction);
        bc = sum * lineBarMinNoticeableFraction + bc * (1.0 - lineBarMinNoticeableFraction);
        nc = nc * (1.0 - lineBarMinNoticeableFraction);
    } else if (!qFuzzyIsNull(mc)) {
        /* Make sure motion is noticeable even if there isn't much of it.
             * Note that these adjustments don't change sum. */
        rc = rc * (1.0 - lineBarMinNoticeableFraction);
        mc = sum * lineBarMinNoticeableFraction + mc * (1.0 - lineBarMinNoticeableFraction);
        nc = nc * (1.0 - lineBarMinNoticeableFraction);
    } else if (!qFuzzyIsNull(rc) && rc < sum * lineBarMinNoticeableFraction) {
        /* Make sure recording content is noticeable even if there isn't much of it.
             * Note that these adjustments don't change sum because mc == 0. */
        rc = sum * lineBarMinNoticeableFraction;// + rc * (1.0 - lineBarMinNoticeableFraction);
        nc = sum * (1.0 - lineBarMinNoticeableFraction);
    }

    return  linearCombine(
                1.0,
                linearCombine(rc / sum, colors[Qn::RecordingContent], mc / sum, colors[Qn::MotionContent]),
                1.0,
                linearCombine(bc / sum, colors[Qn::BookmarksContent], nc / sum, colors[Qn::TimePeriodContentCount])
            );
}
