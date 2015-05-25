#ifndef QNTIMELINECHUNKPAINTER_H
#define QNTIMELINECHUNKPAINTER_H

#include <QtQuick/QSGGeometry>
#include <QtGui/QColor>

#include "recording/time_period.h"

class QnTimelineChunkPainter {
public:
    QnTimelineChunkPainter(QSGGeometry *geometry):
        m_geometry(geometry),
        m_centralPosition(0),
        m_centralCoordinate(0),
        m_minChunkLength(0),
        m_position(0),
        m_pendingLength(0),
        m_pendingPosition(0)
    {
        Q_ASSERT(m_geometry);
        m_color[0] = Qt::darkGreen;
        m_color[1] = Qt::red;
        m_color[2] = Qt::yellow;
        m_color[3] = Qt ::transparent;
    }

    std::array<QColor, Qn::TimePeriodContentCount + 1> colors() const;
    void setColors(const std::array<QColor, Qn::TimePeriodContentCount + 1> &colors);

    void start(qint64 startPos, qint64 centralPos, const QRectF &rect, int chunkCount, qint64 windowStart, qint64 windowEnd);
    void paintChunk(qint64 length, Qn::TimePeriodContent content);
    void stop();

private:
    qreal xFromValue(qint64 value);
    void storeChunk(qint64 length, Qn::TimePeriodContent content);
    void flushChunk();
    void addRect(const QRectF &rect, const QColor &color);
    QColor currentColor(const std::array<QColor, Qn::TimePeriodContentCount + 1> &colors) const;

private:
    QSGGeometry *m_geometry;
    QSGGeometry::ColoredPoint2D *m_points;
    int m_index;

    qint64 m_windowStart;
    qint64 m_windowEnd;
    qint64 m_centralPosition;
    qreal m_centralCoordinate;
    qint64 m_minChunkLength;
    QRect m_rect;

    qint64 m_position;
    qint64 m_pendingLength;
    qint64 m_pendingPosition;

    std::array<qint64, Qn::TimePeriodContentCount + 1> m_weights;
    std::array<QColor, Qn::TimePeriodContentCount + 1> m_color;
};

#endif // QNTIMELINECHUNKPAINTER_H
