#include "time_slider_pixmap_cache.h"

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QFont>

#include <ui/common/text_pixmap_cache.h>

QnTimeSliderPixmapCache::QnTimeSliderPixmapCache(QObject *parent):
    QObject(parent),
    m_font(QApplication::font()),
    m_cache(QnTextPixmapCache::instance()),
    m_pixmapByShortPositionKey(64 * 1024 * 1024), /* These are frequently reused, so we want the cache to be big. */
    m_pixmapByLongPositionKey(2 * 1024 * 1024) /* These are rarely reused once out of scope, so we don't want the cache to be large. */
{}

QnTimeSliderPixmapCache::~QnTimeSliderPixmapCache() {
    m_pixmapByShortPositionKey.clear();
    m_pixmapByLongPositionKey.clear();
}

Q_GLOBAL_STATIC(QnTimeSliderPixmapCache, qn_timeSliderPixmapCache_instance)

QnTimeSliderPixmapCache *QnTimeSliderPixmapCache::instance() {
    return qn_timeSliderPixmapCache_instance();
}

const QPixmap &QnTimeSliderPixmapCache::positionShortPixmap(qint64 position, int height, const QnTimeStep &step) {
    qint32 key = shortCacheKey(position, height, step);

    const QPixmap *result = m_pixmapByShortPositionKey.object(key);
    if(result)
        return *result;

    result = new QPixmap(textPixmap(toShortString(position, step), height, QColor(255, 255, 255, 255)));
    m_pixmapByShortPositionKey.insert(key, result, result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap &QnTimeSliderPixmapCache::positionLongPixmap(qint64 position, int height, const QnTimeStep &step) {
    QnTimeStepLongCacheKey key = longCacheKey(position, height, step);

    const QPixmap *result = m_pixmapByLongPositionKey.object(key);
    if(result)
        return *result;

    result = new QPixmap(textPixmap(toLongString(position, step), height, QColor(255, 255, 255, 255)));
    m_pixmapByLongPositionKey.insert(key, result, result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap &QnTimeSliderPixmapCache::textPixmap(const QString &text, int height, const QColor &color) {
    QFont localFont = m_font;
    localFont.setPixelSize(height);
    
    return m_cache->pixmap(text, localFont, color);
}

