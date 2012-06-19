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
    QnThumbnail(const QPixmap &pixmap, const QSize &size, qint64 time, qint64 actualTime, qint64 timeStep): 
        m_pixmap(pixmap),
        m_size(size),
        m_time(time),
        m_actualTime(actualTime),
        m_timeStep(timeStep)
    {}

    QnThumbnail():
        m_time(0),
        m_actualTime(0),
        m_timeStep(0)
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

private:
    QPixmap m_pixmap;
    QSize m_size;
    qint64 m_time, m_actualTime;
    qint64 m_timeStep;
};

Q_DECLARE_METATYPE(QnThumbnail);

#endif // QN_THUMBNAIL_H

