#include "timeline_chunk_painter.h"

#include <algorithm>

namespace {
    /** Minimal color coefficient for the most noticeable chunk color in range */
    const qreal lineBarMinNoticeableFraction = 0.5;

    QColor linearCombine(qreal a, const QColor &x, qreal b, const QColor &y) {
        return QColor(x.red() * a + y.red() * b,
                      x.green() * a + y.green() * b,
                      x.blue() * a + y.blue() * b,
                      x.alpha() * a + y.alpha() * b);
    }

    QColor makePremultiplied(QColor color)
    {
        color.setRedF(color.redF() * color.alphaF());
        color.setGreenF(color.greenF() * color.alphaF());
        color.setBlueF(color.blueF() * color.alphaF());
        return color;
    }
}

std::array<QColor, Qn::TimePeriodContentCount + 1> QnTimelineChunkPainter::colors() const {
    return m_color;
}

void QnTimelineChunkPainter::setColors(const std::array<QColor, Qn::TimePeriodContentCount + 1> &colors) {
    m_color = colors;
}

void QnTimelineChunkPainter::start(qint64 startPos, const QRectF &rect, int chunkCount, qint64 windowStart, qint64 windowEnd) {
    m_position = startPos;
    m_rect = rect.toRect();
    m_windowStart = windowStart;
    m_windowEnd = windowEnd;
    m_minChunkLength = (windowEnd - windowStart) / rect.width();

    // There cannot be more rectangles than count of pixels
    const int maxRectCount = rect.width();
    int rectCount = qMin(maxRectCount, chunkCount * 2 + 2);
    /* +2 is for the situations like this:
     *            chunk
     *             \/
     * [  +1   +++++++++++   +1  ]
     */
    m_geometry->allocate(rectCount * 6);
    m_points = m_geometry->vertexDataAsColoredPoint2D();
    m_index = 0;

    std::fill(m_weights.begin(), m_weights.end(), 0);
}

void QnTimelineChunkPainter::paintChunk(qint64 length, Qn::TimePeriodContent content) {
    NX_ASSERT(length >= 0);

    if (length < 0)
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
    if (m_index > m_geometry->vertexCount() - 6)
        return;

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

    std::fill(m_weights.begin(), m_weights.end(), 0);
}

void QnTimelineChunkPainter::addRect(const QRectF &rect, const QColor &color)
{
    const auto fixedColor = makePremultiplied(color);
    const auto a = fixedColor.alpha();
    const auto r = fixedColor.red();
    const auto g = fixedColor.green();
    const auto b = fixedColor.blue();

    m_points[m_index + 0].set(rect.left(),  rect.top(), r, g, b, a);
    m_points[m_index + 1].set(rect.right(), rect.top(), r, g, b, a);
    m_points[m_index + 2].set(rect.right(), rect.bottom(), r, g, b, a);
    m_points[m_index + 3] = m_points[m_index + 0];
    m_points[m_index + 4] = m_points[m_index + 2];
    m_points[m_index + 5].set(rect.left(),  rect.bottom(),  r, g, b, a);
    m_index += 6;
}

QColor QnTimelineChunkPainter::currentColor(const std::array<QColor, Qn::TimePeriodContentCount + 1> &colors) const {
    qreal rc = m_weights[Qn::RecordingContent];
    qreal mc = m_weights[Qn::MotionContent];
    qreal nc = m_weights[Qn::TimePeriodContentCount];
    qreal sum = m_pendingLength;

    if (!qFuzzyIsNull(mc)) {
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

return linearCombine(rc / sum, colors[Qn::RecordingContent], 1.0, linearCombine(mc / sum, colors[Qn::MotionContent], nc / sum, colors[Qn::TimePeriodContentCount]));}
