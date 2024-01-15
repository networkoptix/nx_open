// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QCache>
#include <QtCore/QObject>
#include <QtCore/QTimeZone>
#include <QtGui/QFont>
#include <QtGui/QPixmap>

#include "time_step.h"

class QnTextPixmapCache;

/**
 * Pixmap cache for time slider.
 *
 * \note This class is <b>not</t> thread-safe.
 * \see QnTextPixmapCache
 */
class QnTimeSliderPixmapCache: public QObject
{
    Q_OBJECT;
    using milliseconds = std::chrono::milliseconds;

public:
    QnTimeSliderPixmapCache(int numLevels, QObject* parent = nullptr);
    virtual ~QnTimeSliderPixmapCache();

    int numLevels() const;

    const QFont& dateFont() const;
    void setDateFont(const QFont& font);

    const QColor& dateColor() const;
    void setDateColor(const QColor& color);

    const QFont& defaultFont() const;
    void setDefaultFont(const QFont& font);

    const QColor& defaultColor() const;
    void setDefaultColor(const QColor& color);

    const QFont& tickmarkFont(int level) const;
    void setTickmarkFont(int level, const QFont& font);

    const QColor& tickmarkColor(int level) const;
    void setTickmarkColor(int level, const QColor& color);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal value);

    const QPixmap& tickmarkTextPixmap(
        int level,
        milliseconds position,
        int height,
        const QnTimeStep& step,
        const QTimeZone& timeZone);

    const QPixmap& dateTextPixmap(
        milliseconds position,
        int height,
        const QnTimeStep& step,
        const QTimeZone& timeZone);

    const QPixmap& textPixmap(const QString& text, int height);
    const QPixmap& textPixmap(const QString& text, int height, const QColor& color);
    const QPixmap& textPixmap(const QString& text, int height, const QColor& color, const QFont& font);
    const QPixmap& textPixmap(const QString& text, const QColor& color, const QFont& font);

    Q_SLOT void clear();

private:
    QFont m_defaultFont;
    QColor m_defaultColor;

    QFont m_dateFont;
    QColor m_dateColor;

    QVector<QFont> m_tickmarkFonts;
    QVector<QColor> m_tickmarkColors;

    qreal m_devicePixelRatio = 1.0;

    QnTextPixmapCache* const m_cache;

    using ShortKeyCache = QCache<qint32, const QPixmap>;
    QVector<ShortKeyCache*> m_pixmapByShortPositionKey;

    using LongKeyCache = QCache<QnTimeStepLongCacheKey, const QPixmap>;
    LongKeyCache m_pixmapByLongPositionKey;
};
