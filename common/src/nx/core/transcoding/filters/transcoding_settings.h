#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/QRectF>
#include <QtCore/QSharedPointer>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>

#include <core/ptz/item_dewarping_params.h>

#include <utils/common/aspect_ratio.h>
#include <utils/color_space/image_correction.h>

namespace nx {
namespace core {
namespace transcoding {

struct OverlaySettings
{
    enum class Type
    {
        timestamp,
        image,
        text
    };

    virtual ~OverlaySettings() {}
    virtual Type type() const = 0;

    QPoint position;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
};

using OverlaySettingsPtr = QSharedPointer<OverlaySettings>;

struct Settings
{
    // Forced aspect ratio, requires transcoding if not empty.
    QnAspectRatio aspectRatio;

    // Forced rotation, requires transcoding if not zero.
    int rotation;

    // Zoom window region, requires transcoding if not empty.
    QRectF zoomWindow;

    // Item dewarping, requires transcoding if enabled.
    QnItemDewarpingParams dewarping;

    // Image enhancement, requires transcoding if enabled.
    ImageCorrectionParams enhancement;

    QVector<QSharedPointer<OverlaySettings>> overlays;
};

struct TextOverlaySettings: OverlaySettings
{
    QString text;
    int fontSize = 15;
    int overlayWidth = 320;
    int indent = 12;
    QColor foreground = Qt::white;
    QColor background = QColor(0, 0, 0, 0x7F);
    qreal roundingRadius = 4.0;
    virtual Type type() const { return Type::text; }
};

struct ImageOverlaySettings: OverlaySettings
{
    QImage image;
    int overlayWidth = 0;
    qreal opacity = 1.0;
    virtual Type type() const { return Type::image; }
};

struct TimestampOverlaySettings: OverlaySettings
{
    Qt::DateFormat format = Qt::DefaultLocaleLongDate;
    int fontSize = 18;
    QColor foreground = Qt::white;
    QColor shadow = Qt::black;
    // Use client time to ensure WYSIWYG if needed.
    qint64 serverTimeDisplayOffset = 0;
    virtual Type type() const { return Type::timestamp; }
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
