#pragma once

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <transcoding/filters/filter_helper.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportTextOverlaySettings
{
    QString text;
    int fontSize = 15;
    int overlayWidth = 320;
    int indent = 12;
    QColor foreground = Qt::white;
    QColor background = QColor(0, 0, 0, 0x7F);
    qreal roundingRadius = 4.0;
};

struct ExportImageOverlaySettings
{
    QImage image;
    QString name;
    int overlayWidth = 0;
    qreal opacity = 1.0;
    QColor background = Qt::transparent;
};

struct ExportMediaSettings
{
    QnMediaResourcePtr mediaResource;
    QnTimePeriod timePeriod;
    QString fileName;
    QnImageFilterHelper imageParameters;
    qint64 serverTimeZoneMs = 0;
    qint64 timelapseFrameStepMs = 0; //< 0 means disabled timelapse.

    ExportTextOverlaySettings textOverlay;
    ExportImageOverlaySettings imageOverlay;
};

} // namespace desktop
} // namespace client
} // namespace nx
