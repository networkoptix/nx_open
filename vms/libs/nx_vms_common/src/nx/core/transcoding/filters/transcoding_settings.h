// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QSharedPointer>
#include <QtCore/QTimeZone>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>

#include <core/resource/resource_media_layout.h>
#include <nx/core/watermark/watermark.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <nx/vms/api/data/pixelation_settings.h>
#include <utils/common/aspect_ratio.h>

#include "../timestamp_format.h"
#include "nx/vms/common/pixelation/pixelation_settings.h"

namespace nx::core::transcoding {

struct OverlaySettings
{
    enum class Type
    {
        timestamp,
        image,
        watermark,
        text,
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
    nx::vms::api::dewarping::ViewData dewarping;

    // Dewarping params.
    nx::vms::api::dewarping::MediaData dewarpingMedia;

    // Image enhancement, requires transcoding if enabled.
    nx::vms::api::ImageCorrectionData enhancement;

    // Watermark; sometimes it behaves different from other overlays.
    Watermark watermark;

    std::optional<nx::vms::api::PixelationSettings> pixelationSettings;

    QVector<OverlaySettingsPtr> overlays;

    // Video layout for tiled image filter.
    QnConstResourceVideoLayoutPtr layout;

    // Force transcoding
    bool forceTranscoding = false;


    /**
     * Check if options require transcoding.
     * @param concernTiling If video layout transcoding (tiling) matters - it is not applied while
     *     transcoding images.
     */
    inline bool isTranscodingRequired(bool concernTiling = true) const
    {
        if (concernTiling && layout && layout->channelCount() > 1)
            return true;

        if (forceTranscoding)
            return true;

        if (aspectRatio.isValid())
            return true;

        if (!zoomWindow.isEmpty())
            return true;

        if (dewarping.enabled)
            return true;

        if (enhancement.enabled)
            return true;

        if (rotation != 0)
            return true;

        if (watermark.visible())
            return true;

        if (pixelationSettings.has_value()
            && (pixelationSettings->isAllObjectTypes || !pixelationSettings->objectTypeIds.empty()))
            return true;

        return !overlays.isEmpty();
    }
};

struct ImageOverlaySettings: OverlaySettings
{
    QImage image;
    virtual Type type() const override { return Type::image; }
};

struct TimestampOverlaySettings: OverlaySettings
{
    TimestampFormat format = TimestampFormat::longDate;
    int fontSize = 18;
    QColor foreground = Qt::white;
    QColor outline = Qt::black;
    QTimeZone timeZone{QTimeZone::LocalTime}; //< Use client time to ensure WYSIWYG if needed.
    virtual Type type() const override { return Type::timestamp; }
};

struct TextOverlaySettings: OverlaySettings
{
    QString text;

    virtual Type type() const override { return Type::text; }
};


} // namespace nx::core::transcoding
