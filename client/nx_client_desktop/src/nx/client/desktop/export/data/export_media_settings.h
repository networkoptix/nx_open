#pragma once

#include <QtCore/QRectF>
#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include <core/ptz/item_dewarping_params.h>

#include <recording/time_period.h>

#include <utils/common/aspect_ratio.h>
#include <utils/color_space/image_correction.h>

#include <nx/client/desktop/common/utils/filesystem.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportOverlaySettings
{
    enum class Type
    {
        timestamp,
        image,
        text
    };

    virtual ~ExportOverlaySettings() {}
    virtual Type type() const = 0;

    QPointF position;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
};

struct ExportMediaSettings
{
    QnMediaResourcePtr mediaResource;
    QnTimePeriod timePeriod;
    Filename fileName;

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

    qint64 serverTimeZoneMs = 0;

    qint64 timelapseFrameStepMs = 0; //< 0 means disabled timelapse.
    QVector<QSharedPointer<ExportOverlaySettings>> overlays;
};

struct ExportTextOverlaySettings: ExportOverlaySettings
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

struct ExportImageOverlaySettings: ExportOverlaySettings
{
    QImage image;
    int overlayWidth = 0;
    qreal opacity = 1.0;
    virtual Type type() const { return Type::image; }
};

struct ExportTimestampOverlaySettings: ExportOverlaySettings
{
    Qt::DateFormat format = Qt::DefaultLocaleLongDate;
    int fontSize = 18;
    QColor foreground = Qt::white;
    QColor shadow = Qt::black;
    // Use client time to ensure WYSIWYG if needed.
    qint64 serverTimeDisplayOffset = 0;
    virtual Type type() const { return Type::timestamp; }
};

} // namespace desktop
} // namespace client
} // namespace nx
