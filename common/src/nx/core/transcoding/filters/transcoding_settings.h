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
        image
    };

    virtual ~OverlaySettings() {}
    virtual Type type() const = 0;

    QPoint offset;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
};

using OverlaySettingsPtr = QSharedPointer<OverlaySettings>;

struct Settings
{
    // Forced aspect ratio, requires transcoding if not empty.
    QnAspectRatio aspectRatio;

    // Forced rotation, requires transcoding if not zero.
    int rotation = 0;

    // Zoom window region, requires transcoding if not empty.
    QRectF zoomWindow;

    // Item dewarping, requires transcoding if enabled.
    QnItemDewarpingParams dewarping;

    // Image enhancement, requires transcoding if enabled.
    ImageCorrectionParams enhancement;

    QVector<OverlaySettingsPtr> overlays;
};

struct ImageOverlaySettings: OverlaySettings
{
    QImage image;
    virtual Type type() const { return Type::image; }
};

struct TimestampOverlaySettings: OverlaySettings
{
    Qt::DateFormat format = Qt::DefaultLocaleLongDate;
    int fontSize = 18;
    QColor foreground = Qt::white;
    QColor outline = Qt::black;
    // Use client time to ensure WYSIWYG if needed.
    qint64 serverTimeDisplayOffsetMs = 0;
    virtual Type type() const { return Type::timestamp; }
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
