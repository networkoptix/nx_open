#ifndef QN_TIME_SLIDER_PIXMAP_CACHE_H
#define QN_TIME_SLIDER_PIXMAP_CACHE_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <QtGui/QPixmap>
#include <QtGui/QFont>

#include <utils/common/hash.h>

#include "time_step.h"

class QnTextPixmapCache;

/**
 * Global pixmap cache for time slider.
 * 
 * \see                                 QnTextPixmapCache
 */
class QnTimeSliderPixmapCache: public QObject {
    Q_OBJECT;
public:
    QnTimeSliderPixmapCache(QObject *parent = NULL);
    virtual ~QnTimeSliderPixmapCache();

    static QnTimeSliderPixmapCache *instance();

    const QPixmap *positionShortPixmap(qint64 position, int height, const QnTimeStep &step);
    const QPixmap *positionLongPixmap(qint64 position, int height, const QnTimeStep &step);
    const QPixmap *textPixmap(const QString &text, int height, const QColor &color);

private:
    QFont m_font;
    QnTextPixmapCache *m_cache;
    QHash<qint32, const QPixmap *> m_pixmapByShortPositionKey;
    QHash<QnTimeStepLongCacheKey, const QPixmap *> m_pixmapByLongPositionKey;
};

#endif // QN_TIME_SLIDER_PIXMAP_CACHE_H
