// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_slider_pixmap_cache.h"

#include <chrono>

#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <platform/platform_abstraction.h>
#include <ui/common/text_pixmap_cache.h>

using std::chrono::milliseconds;

QnTimeSliderPixmapCache::QnTimeSliderPixmapCache(int numLevels, QObject *parent):
    QObject(parent),
    m_defaultFont(QApplication::font()),
    m_defaultColor(nx::vms::client::core::colorTheme()->color("light1", 255)),
    m_dateFont(m_defaultFont),
    m_dateColor(m_defaultColor),
    m_tickmarkFonts(numLevels, m_defaultFont),
    m_tickmarkColors(numLevels, m_defaultColor),
    m_cache(QnTextPixmapCache::instance()),
    m_pixmapByShortPositionKey(numLevels),
    // These are rarely reused once out of scope, so we don't want the cache to be large.
    m_pixmapByLongPositionKey(2 * 1024 * 1024)
{
    // These are frequently reused, so we want the cache to be big.
    for (auto& newCache : m_pixmapByShortPositionKey)
        newCache = new ShortKeyCache(64 * 1024 * 1024);

    connect(qnPlatform->notifier(), &QnPlatformNotifier::timeZoneChanged,
        this, &QnTimeSliderPixmapCache::clear);
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

qreal QnTimeSliderPixmapCache::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

void QnTimeSliderPixmapCache::setDevicePixelRatio(qreal value)
{
    m_devicePixelRatio = value;
}

const QPixmap& QnTimeSliderPixmapCache::tickmarkTextPixmap(
    int level,
    milliseconds position,
    int height,
    const QnTimeStep& step,
    const QTimeZone& timeZone)
{
    level = qBound(0, level, numLevels() - 1);
    qint32 key = shortCacheKey(position, qRound(height * m_devicePixelRatio), step, timeZone);

    const QPixmap* result = m_pixmapByShortPositionKey[level]->object(key);
    if (result)
        return *result;

    result = new QPixmap(textPixmap(toShortString(position, step, timeZone), height,
        m_tickmarkColors[level], m_tickmarkFonts[level]));
    m_pixmapByShortPositionKey[level]->insert(key, result,
        result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap& QnTimeSliderPixmapCache::dateTextPixmap(
    milliseconds position,
    int height,
    const QnTimeStep& step,
    const QTimeZone& timeZone)
{
    QnTimeStepLongCacheKey key = longCacheKey(position, qRound(height * m_devicePixelRatio), step);

    const QPixmap* result = m_pixmapByLongPositionKey.object(key);
    if (result)
        return *result;

    result = new QPixmap(
        textPixmap(toLongString(position, step, timeZone), height, m_dateColor, m_dateFont));
    m_pixmapByLongPositionKey.insert(
        key, result, result->width() * result->height() * result->depth() / 8);
    return *result;
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(const QString& text, int height)
{
    return textPixmap(text, height, m_defaultColor);
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(
    const QString& text, int height, const QColor& color)
{
    return textPixmap(text, height, color, m_defaultFont);
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(
    const QString& text, int height, const QColor& color, const QFont& font)
{
    QFont localFont(font);
    localFont.setPixelSize(height);
    return textPixmap(text, color, localFont);
}

const QPixmap& QnTimeSliderPixmapCache::textPixmap(
    const QString& text, const QColor& color, const QFont& font)
{
    return m_cache->pixmap(text, font, color, m_devicePixelRatio);
}

void QnTimeSliderPixmapCache::clear()
{
    m_pixmapByLongPositionKey.clear();

    for (auto cache : m_pixmapByShortPositionKey)
        cache->clear();
}
