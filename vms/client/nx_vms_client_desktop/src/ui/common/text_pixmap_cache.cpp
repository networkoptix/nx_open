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

#include <ui/utils/blur_image.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

#include "hashed_font.h"
#include <utils/common/hash.h>

namespace {

QImage getShadowImage(
    const QString& text,
    const QSize& size,
    qreal pixelRatio,
    const QPointF& origin,
    const QFont& font,
    qreal radius)
{
    static const QColor kShadowColor = qRgba(0, 0, 0, 255);

    // Use Format_ARGB32_Premultiplied so qt_blurImage() won't convert the image before blurring.
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(pixelRatio);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setPen(kShadowColor);
    painter.setFont(font);
    const QPointF offset{0, 0.75};
    painter.drawText(-origin + offset, text);
    painter.end();

    qt_blurImage(image, radius, false);
    return image;
}

QnTextPixmap renderText(const QString& text, const QPen& pen, const QFont& font,
    qreal devicePixelRatio, int width, Qt::TextElideMode elideMode, qreal shadowRadius = 0.0)
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

    const auto pixmapSize = (QSizeF(size) * devicePixelRatio).toSize();

    QPixmap pixmap(pixmapSize);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    if (shadowRadius > 0)
    {
        const auto shadowImage =
            getShadowImage(renderText, pixmapSize, devicePixelRatio, origin, font, shadowRadius);
        painter.drawImage(QPoint{0, 0}, shadowImage);
    }

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
        qreal devicePixelRatio = 1.0;

        Key(const QString& text, const QFont& font, const QColor& color, qreal devicePixelRatio):
            text(text),
            font(font),
            color(color),
            devicePixelRatio(devicePixelRatio)
        {
        }

        friend size_t qHash(const Key& key, uint seed = 0)
        {
            return qHashMulti(seed, key.text, key.font, key.color, key.devicePixelRatio);
        }

        bool operator == (const Key& other) const
        {
            return text == other.text
                && font == other.font
                && color == other.color
                && qFuzzyEquals(devicePixelRatio, other.devicePixelRatio);
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
    const QColor& color,
    qreal devicePixelRatio,
    qreal shadowRadius)
{
    return pixmap(text, font, color, devicePixelRatio, 0, Qt::ElideNone, shadowRadius).pixmap;
}

const QnTextPixmap& QnTextPixmapCache::pixmap(
    const QString& text,
    const QFont& font,
    const QColor& color,
    qreal devicePixelRatio,
    int width,
    Qt::TextElideMode elideMode,
    qreal shadowRadius)
{
    if (!NX_ASSERT(!qFuzzyIsNull(devicePixelRatio)))
        devicePixelRatio = qApp->devicePixelRatio();

    QColor localColor = color.toRgb();
    if (!localColor.isValid())
        localColor = QColor(0, 0, 0, 255);

    QnTextPixmapCachePrivate::Key key(text, font, localColor, devicePixelRatio);
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

    const auto textPixmapStruct = renderText(
        text, QPen(localColor, 0), font, devicePixelRatio, width, elideMode, shadowRadius);

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
