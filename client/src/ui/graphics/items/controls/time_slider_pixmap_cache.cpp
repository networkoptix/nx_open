#include "time_slider_pixmap_cache.h"

#include <QtWidgets/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QFont>

#include <ui/common/text_pixmap_cache.h>
#include <platform/platform_abstraction.h>

QnTimeSliderPixmapCache::QnTimeSliderPixmapCache(QObject *parent):
    QObject(parent),
    m_font(QApplication::font()),
    m_cache(QnTextPixmapCache::instance()),
    m_pixmapByShortPositionKey(64 * 1024 * 1024), /* These are frequently reused, so we want the cache to be big. */
    m_pixmapByLongPositionKey(2 * 1024 * 1024) /* These are rarely reused once out of scope, so we don't want the cache to be large. */
{
    connect(qnPlatform->notifier(), &QnPlatformNotifier::timeZoneChanged, this, &QnTimeSliderPixmapCache::clear);
}

QnTimeSliderPixmapCache::~QnTimeSliderPixmapCache() {
    m_pixmapByShortPositionKey.clear();
    m_pixmapByLongPositionKey.clear();
}

const QFont &QnTimeSliderPixmapCache::font() const {
    return m_font;
}

void QnTimeSliderPixmapCache::setFont(const QFont &font) {
    if(m_font == font)
        return;

    m_font = font;

    clear();
}

const QColor &QnTimeSliderPixmapCache::color() const {
    return m_color;
}

void QnTimeSliderPixmapCache::setColor(const QColor &color) {
    if(m_color == color)
        return;

    m_color = color;

    clear();
}

const QPixmap &QnTimeSliderPixmapCache::positionShortPixmap(qint64 position, int height, const QnTimeStep &step) {
    qint32 key = shortCacheKey(position, height, step);

    const QPixmap *result = m_pixmapByShortPositionKey.object(key);
    if(result)
        return *result;

    result = new QPixmap(textPixmap(toShortString(position, step), height));
    m_pixmapByShortPositionKey.insert(key, result, result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap &QnTimeSliderPixmapCache::positionLongPixmap(qint64 position, int height, const QnTimeStep &step) {
    QnTimeStepLongCacheKey key = longCacheKey(position, height, step);

    const QPixmap *result = m_pixmapByLongPositionKey.object(key);
    if(result)
        return *result;

    result = new QPixmap(textPixmap(toLongString(position, step), height));
    m_pixmapByLongPositionKey.insert(key, result, result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap &QnTimeSliderPixmapCache::textPixmap(const QString &text, int height) {
    QFont localFont = m_font;
    localFont.setPixelSize(height);
    
    return m_cache->pixmap(text, localFont, m_color);
}

void QnTimeSliderPixmapCache::clear() {
    m_pixmapByLongPositionKey.clear();
    m_pixmapByShortPositionKey.clear();
}

