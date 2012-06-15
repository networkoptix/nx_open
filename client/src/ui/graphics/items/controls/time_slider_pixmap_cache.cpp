#include "time_slider_pixmap_cache.h"

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QFont>

#include <ui/common/text_pixmap_cache.h>

QnTimeSliderPixmapCache::QnTimeSliderPixmapCache(QObject *parent):
    QObject(parent),
    m_font(QApplication::font()),
    m_cache(QnTextPixmapCache::instance())
{}

QnTimeSliderPixmapCache::~QnTimeSliderPixmapCache() {
    m_pixmapByShortPositionKey.clear();
    m_pixmapByLongPositionKey.clear();
}

Q_GLOBAL_STATIC(QnTimeSliderPixmapCache, qn_timeSliderPixmapCache_instance)

QnTimeSliderPixmapCache *QnTimeSliderPixmapCache::instance() {
    return qn_timeSliderPixmapCache_instance();
}

const QPixmap *QnTimeSliderPixmapCache::positionShortPixmap(qint64 position, int height, const QnTimeStep &step) {
    qint32 key = shortCacheKey(position, height, step);

    QHash<qint32, const QPixmap *>::const_iterator itr = m_pixmapByShortPositionKey.find(key);
    if(itr != m_pixmapByShortPositionKey.end())
        return *itr;

    return m_pixmapByShortPositionKey[key] = textPixmap(toShortString(position, step), height, QColor(255, 255, 255, 255));
}

const QPixmap *QnTimeSliderPixmapCache::positionLongPixmap(qint64 position, int height, const QnTimeStep &step) {
    QnTimeStepLongCacheKey key = longCacheKey(position, height, step);

    QHash<QnTimeStepLongCacheKey, const QPixmap *>::const_iterator itr = m_pixmapByLongPositionKey.find(key);
    if(itr != m_pixmapByLongPositionKey.end())
        return *itr;

    return m_pixmapByLongPositionKey[key] = textPixmap(toLongString(position, step), height, QColor(255, 255, 255, 255));
}

const QPixmap *QnTimeSliderPixmapCache::textPixmap(const QString &text, int height, const QColor &color) {
    QFont localFont = m_font;
    localFont.setPixelSize(height);
    
    return m_cache->pixmap(text, localFont, color);
}

