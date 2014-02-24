#ifndef QN_TIME_SLIDER_PIXMAP_CACHE_H
#define QN_TIME_SLIDER_PIXMAP_CACHE_H

#include <QtCore/QObject>
#include <QtCore/QCache>
#include <QtGui/QPixmap>
#include <QtGui/QFont>

#include "time_step.h"

class QnTextPixmapCache;

/**
 * Pixmap cache for time slider.
 * 
 * \note This class is <b>not</t> thread-safe.
 * \see QnTextPixmapCache
 */
class QnTimeSliderPixmapCache: public QObject {
    Q_OBJECT;
public:
    QnTimeSliderPixmapCache(QObject *parent = NULL);
    virtual ~QnTimeSliderPixmapCache();

    const QFont &font() const;
    void setFont(const QFont &font);

    const QPixmap &positionShortPixmap(qint64 position, int height, const QnTimeStep &step, const QColor &color);
    const QPixmap &positionLongPixmap(qint64 position, int height, const QnTimeStep &step, const QColor &color);
    const QPixmap &textPixmap(const QString &text, int height, const QColor &color);

    /**
     * Clears positional part of this cache.
     * 
     * Note that underlying <tt>QString->QPixmap</tt> cache is not cleared.
     */
    Q_SLOT void clear();

private:
    QFont m_font;
    QnTextPixmapCache *m_cache;
    QCache<qint32, const QPixmap> m_pixmapByShortPositionKey;
    QCache<QnTimeStepLongCacheKey, const QPixmap> m_pixmapByLongPositionKey;
    QPixmap m_nullPixmap;
};

#endif // QN_TIME_SLIDER_PIXMAP_CACHE_H
