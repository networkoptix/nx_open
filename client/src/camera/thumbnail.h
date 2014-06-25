#ifndef QN_THUMBNAIL_H
#define QN_THUMBNAIL_H

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtGui/QImage>

#include <core/datapacket/video_data_packet.h>

#include <ui/common/geometry.h>

/**
 * Value class representing a single video thumbnail.
 */
class QnThumbnail {
public:
    QnThumbnail(const QnCompressedVideoDataPtr &data, qint64 time, qint64 actualTime, qint64 timeStep, int generation):
        m_data(data),
        m_time(time),
        m_actualTime(actualTime),
        m_timeStep(timeStep),
        m_generation(generation)
    {}

    QnThumbnail(const QImage &image, const QSize &size, qint64 time, qint64 actualTime, qint64 timeStep, int generation): 
        m_image(image),
        m_size(size),
        m_time(time),
        m_actualTime(actualTime),
        m_timeStep(timeStep),
        m_generation(generation)
    {}

    QnThumbnail():
        m_time(0),
        m_actualTime(0),
        m_timeStep(0)
    {}

    QnThumbnail(const QnThumbnail &other, qint64 time):
        m_data(other.m_data),
        m_image(other.m_image),
        m_size(other.m_size),
        m_time(time),
        m_actualTime(other.m_actualTime),
        m_timeStep(other.m_timeStep),
        m_generation(other.m_generation)
    {}

    bool isEmpty() const {
        return m_data.isNull() && m_image.isNull();
    }

    const QnCompressedVideoDataPtr &data() const {
        return m_data;
    }

    const QImage &image() const {
        return m_image;
    }

    const QSize &size() const {
        return m_size;
    }

    qreal aspectRatio() const {
        return QnGeometry::aspectRatio(m_size);
    }

    qint64 time() const {
        return m_time;
    }

    qint64 actualTime() const {
        return m_actualTime;
    }

    qint64 timeStep() const {
        return m_timeStep;
    }

    int generation() const {
        return m_generation;
    }

private:
    QnCompressedVideoDataPtr m_data;
    QImage m_image;
    QSize m_size;
    qint64 m_time, m_actualTime;
    qint64 m_timeStep;
    int m_generation;
};

Q_DECLARE_METATYPE(QnThumbnail);

#endif // QN_THUMBNAIL_H

