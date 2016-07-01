#include "time_slider_pixmap_cache.h"

#include <QtWidgets/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QFont>

#include <ui/common/text_pixmap_cache.h>
#include <platform/platform_abstraction.h>

QnTimeSliderPixmapCache::QnTimeSliderPixmapCache(int numLevels, QObject *parent):
    QObject(parent),
    m_defaultFont(QApplication::font()),
    m_defaultColor(QColor(255, 255, 255, 255)),
    m_dateFont(m_defaultFont),
    m_dateColor(m_defaultColor),
    m_tickmarkFonts(numLevels, m_defaultFont),
    m_tickmarkColors(numLevels, m_defaultColor),
    m_cache(QnTextPixmapCache::instance()),
    m_pixmapByShortPositionKey(numLevels),
    m_pixmapByLongPositionKey(2 * 1024 * 1024) /* These are rarely reused once out of scope, so we don't want the cache to be large. */
{
    for (auto& newCache : m_pixmapByShortPositionKey)
        newCache = new ShortKeyCache(64 * 1024 * 1024); /* These are frequently reused, so we want the cache to be big. */

    connect(qnPlatform->notifier(), &QnPlatformNotifier::timeZoneChanged, this, &QnTimeSliderPixmapCache::clear);
}

QnTimeSliderPixmapCache::~QnTimeSliderPixmapCache()
{
    for (auto cache : m_pixmapByShortPositionKey)
        delete cache;
}

int QnTimeSliderPixmapCache::numLevels() const
{
    return m_pixmapByShortPositionKey.size();
}

const QFont& QnTimeSliderPixmapCache::defaultFont() const
{
    return m_defaultFont;
}

void QnTimeSliderPixmapCache::setDefaultFont(const QFont& font)
{
    m_defaultFont = font;
}

const QColor& QnTimeSliderPixmapCache::defaultColor() const
{
    return m_defaultColor;
}

void QnTimeSliderPixmapCache::setDefaultColor(const QColor& color)
{
    m_defaultColor = color;
}

const QFont& QnTimeSliderPixmapCache::dateFont() const
{
    return m_dateFont;
}

void QnTimeSliderPixmapCache::setDateFont(const QFont& font)
{
    if (m_dateFont != font)
    {
        m_dateFont = font;
        m_pixmapByLongPositionKey.clear();
    }
}

const QColor& QnTimeSliderPixmapCache::dateColor() const
{
    return m_dateColor;
}

void QnTimeSliderPixmapCache::setDateColor(const QColor& color)
{
    if (m_dateColor != color)
    {
        m_dateColor = color;
        m_pixmapByLongPositionKey.clear();
    }
}

const QFont& QnTimeSliderPixmapCache::tickmarkFont(int level) const
{
    NX_ASSERT(level >= 0 && level < numLevels());
    return m_tickmarkFonts[level];
}

void QnTimeSliderPixmapCache::setTickmarkFont(int level, const QFont& font)
{
    NX_ASSERT(level >= 0 && level < numLevels());
    if (m_tickmarkFonts[level] != font)
    {
        m_tickmarkFonts[level] = font;
        m_pixmapByShortPositionKey[level]->clear();
    }
}

const QColor& QnTimeSliderPixmapCache::tickmarkColor(int level) const
{
    NX_ASSERT(level >= 0 && level < numLevels());
    return m_tickmarkColors[level];
}

void QnTimeSliderPixmapCache::setTickmarkColor(int level, const QColor& color)
{
    NX_ASSERT(level >= 0 && level < numLevels());
    if (m_tickmarkColors[level] != color)
    {
        m_tickmarkColors[level] = color;
        m_pixmapByShortPositionKey[level]->clear();
    }
}

const QPixmap& QnTimeSliderPixmapCache::tickmarkTextPixmap(int level, qint64 position, int height, const QnTimeStep& step)
{
    level = qBound(0, level, numLevels() - 1);
    qint32 key = shortCacheKey(position, height, step);

    const QPixmap *result = m_pixmapByShortPositionKey[level]->object(key);
    if (result)
        return *result;

    result = new QPixmap(textPixmap(toShortString(position, step), height, m_tickmarkColors[level], m_tickmarkFonts[level]));
    m_pixmapByShortPositionKey[level]->insert(key, result, result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap& QnTimeSliderPixmapCache::dateTextPixmap(qint64 position, int height, const QnTimeStep& step)
{
    QnTimeStepLongCacheKey key = longCacheKey(position, height, step);

    const QPixmap *result = m_pixmapByLongPositionKey.object(key);
    if (result)
        return *result;

    result = new QPixmap(textPixmap(toLongString(position, step), height, m_dateColor, m_dateFont));
    m_pixmapByLongPositionKey.insert(key, result, result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(const QString& text, int height)
{
    return textPixmap(text, height, m_defaultColor);
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(const QString& text, int height, const QColor& color)
{
    return textPixmap(text, height, color, m_defaultFont);
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(const QString& text, int height, const QColor& color, const QFont& font)
{
    QFont localFont(font);
    localFont.setPixelSize(height);
    return textPixmap(text, color, localFont);
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(const QString& text, const QColor& color, const QFont& font)
{
    return m_cache->pixmap(text, font, color);
}

void QnTimeSliderPixmapCache::clear()
{
    m_pixmapByLongPositionKey.clear();

    for (auto cache : m_pixmapByShortPositionKey)
        cache->clear();
}
