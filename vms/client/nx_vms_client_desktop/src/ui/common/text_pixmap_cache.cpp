// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_pixmap_cache.h"

#include <limits>
#include <cmath>

#include <QtCore/QCache>

#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>

#include <nx/utils/log/assert.h>

#include "hashed_font.h"
#include <utils/common/hash.h>

namespace {

QnTextPixmap renderText(const QString& text, const QPen& pen, const QFont& font,
    int width, Qt::TextElideMode elideMode)
{
    const QFontMetrics metrics(font);

    QString renderText = width > 0
        ? metrics.elidedText(text, elideMode, width)
        : text;

    if (renderText.isEmpty())
        return QnTextPixmap();

    renderText.squeeze(); //< Do not bloat cache with excessive allocated data.

    const QSize size = metrics.size(0, renderText);

    if (size.isEmpty())
        return QnTextPixmap();

    const QPoint origin(metrics.leftBearing(renderText[0]), -metrics.ascent());

    const auto pixelRatio = qApp->devicePixelRatio();
    const auto pixmapSize = size * pixelRatio;

    QPixmap pixmap(pixmapSize);
    pixmap.setDevicePixelRatio(pixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setPen(pen);
    painter.setFont(font);
    painter.drawText(-origin, renderText);
    painter.end();

    const int elidedForWidth = (width > 0 && renderText != text) ? width : 0;
    return QnTextPixmap(pixmap, origin, renderText, elideMode, elidedForWidth);
}

int calculateTextPixmapCost(const QnTextPixmap& textPixmap)
{
    static constexpr int kBitsPerByte = 8;
    const QPixmap& pixmap = textPixmap.pixmap;

    int result = pixmap.devicePixelRatio() * pixmap.devicePixelRatio()
        * pixmap.height() * pixmap.width()
        * (pixmap.depth() / kBitsPerByte);

    result += textPixmap.renderedText.capacity() * sizeof(QChar);
    result += sizeof(QnTextPixmap);

    return result;
}

} // namespace

class QnTextPixmapCachePrivate
{
public:
    QnTextPixmapCachePrivate():
        pixmapByKey(64 * 1024 * 1024) /* 64 Megabytes. */
    {
    }

    virtual ~QnTextPixmapCachePrivate()
    {
    }

    struct Key
    {
        QString text;
        QnHashedFont font;
        QColor color;

        Key(const QString& text, const QFont& font, const QColor& color):
            text(text), font(font), color(color)
        {
        }

        friend uint qHash(const Key& key)
        {
            using ::qHash;
            return
                (929 * qHash(key.text)) ^
                (883 * qHash(key.font)) ^
                (547 * qHash(key.color));
        }

        bool operator == (const Key& other) const
        {
            return text == other.text && font == other.font && color == other.color;
        }
    };

    QCache<Key, const QnTextPixmap> pixmapByKey;
};


QnTextPixmapCache::QnTextPixmapCache(QObject* parent):
    QObject(parent),
    d(new QnTextPixmapCachePrivate())
{
}

QnTextPixmapCache::~QnTextPixmapCache()
{
}

const QPixmap& QnTextPixmapCache::pixmap(
    const QString& text,
    const QFont& font,
    const QColor& color)
{
    return pixmap(text, font, color, 0, Qt::ElideNone).pixmap;
}

const QnTextPixmap& QnTextPixmapCache::pixmap(
    const QString& text,
    const QFont& font,
    const QColor& color,
    int width,
    Qt::TextElideMode elideMode)
{
    QColor localColor = color.toRgb();
    if (!localColor.isValid())
        localColor = QColor(0, 0, 0, 255);

    QnTextPixmapCachePrivate::Key key(text, font, localColor);
    auto result = d->pixmapByKey.object(key);
    if (result)
    {
        const bool suitableElideMode = result->elideMode == elideMode || !result->elided();
        if (suitableElideMode)
        {
            static const auto effectiveWidthLimit =
                [](int width)
                {
                    return width > 0
                        ? width
                        : std::numeric_limits<int>::max();
                };

            const int requestLimit = effectiveWidthLimit(width);
            const int resultLimit = effectiveWidthLimit(result->elidedForWidth);

            if (requestLimit > result->size().width() && requestLimit <= resultLimit)
                return *result;
        }

        /* To ensure there's no deep copying: */
        d->pixmapByKey.remove(key);
    }

    const auto textPixmapStruct = renderText(text, QPen(localColor, 0), font, width, elideMode);

    static const QnTextPixmap kEmptyTextPixmap;

    if (textPixmapStruct.pixmap.isNull())
        return kEmptyTextPixmap;

    const auto cost = calculateTextPixmapCost(textPixmapStruct);
    if (d->pixmapByKey.insert(key, result = new QnTextPixmap(std::move(textPixmapStruct)), cost))
        return *result;

    NX_ASSERT(false, "Too huge text pixmap");
    return kEmptyTextPixmap;
}

Q_GLOBAL_STATIC(QnTextPixmapCache, qn_textPixmapCache_instance)

QnTextPixmapCache* QnTextPixmapCache::instance()
{
    return qn_textPixmapCache_instance();
}
