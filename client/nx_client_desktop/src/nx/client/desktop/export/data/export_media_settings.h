#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <transcoding/filters/filter_helper.h>

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
    QnImageFilterHelper imageParameters;
    qint64 serverTimeZoneMs = 0;
    qint64 timelapseFrameStepMs = 0; //< 0 means disabled timelapse.
    QVector<QSharedPointer<ExportOverlaySettings>> overlays;
};

struct ExportTextOverlaySettings: public ExportOverlaySettings
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

struct ExportImageOverlaySettings: public ExportOverlaySettings
{
    QImage image;
    int overlayWidth = 0;
    qreal opacity = 1.0;
    virtual Type type() const { return Type::image; }
};

struct ExportTimestampOverlaySettings: public ExportOverlaySettings
{
    Qt::DateFormat format = Qt::DefaultLocaleLongDate;
    int fontSize = 18;
    QColor foreground = Qt::white;
    QColor shadow = Qt::black;
    virtual Type type() const { return Type::timestamp; }
};

} // namespace desktop
} // namespace client
} // namespace nx
