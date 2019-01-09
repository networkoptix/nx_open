#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/QRectF>
#include <QtCore/QSharedPointer>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>

#include <nx/core/watermark/watermark.h>

#include <utils/common/aspect_ratio.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

namespace nx {
namespace core {
namespace transcoding {

struct OverlaySettings
{
    enum class Type
    {
        timestamp,
        image,
        watermark,
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
    nx::vms::api::DewarpingData dewarping;

    // Image enhancement, requires transcoding if enabled.
    nx::vms::api::ImageCorrectionData enhancement;

    // Watermark; sometimes it behaves different from other overlays.
    Watermark watermark;

    QVector<OverlaySettingsPtr> overlays;
};

struct ImageOverlaySettings: OverlaySettings
{
    QImage image;
    virtual Type type() const override { return Type::image; }
};

struct TimestampOverlaySettings: OverlaySettings
{
    Qt::DateFormat format = Qt::DefaultLocaleLongDate;
    int fontSize = 18;
    QColor foreground = Qt::white;
    QColor outline = Qt::black;
    // Use client time to ensure WYSIWYG if needed.
    qint64 serverTimeDisplayOffsetMs = 0;
    virtual Type type() const override { return Type::timestamp; }
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
