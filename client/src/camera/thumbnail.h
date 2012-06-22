#ifndef QN_THUMBNAIL_H
#define QN_THUMBNAIL_H

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtGui/QPixmap>

/**
 * Value class representing a single video thumbnail.
 */
class QnThumbnail {
public:
    QnThumbnail(const QPixmap &pixmap, const QSize &size, qint64 time, qint64 actualTime, qint64 timeStep, int generation): 
        m_pixmap(pixmap),
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
        m_pixmap(other.m_pixmap),
        m_size(other.m_size),
        m_time(time),
        m_actualTime(other.m_actualTime),
        m_timeStep(other.m_timeStep),
        m_generation(other.m_generation)
    {}

    bool isEmpty() const {
        return m_pixmap.isNull();
    }

    const QPixmap &pixmap() const {
        return m_pixmap;
    }

    const QSize &size() const {
        return m_size;
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
    QPixmap m_pixmap;
    QSize m_size;
    qint64 m_time, m_actualTime;
    qint64 m_timeStep;
    int m_generation;
};

Q_DECLARE_METATYPE(QnThumbnail);

#endif // QN_THUMBNAIL_H

