// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "widget_analytics_controller.h"

#include <optional>

#include <QtCore/QPointer>

#include <analytics/common/object_metadata.h>
#include <analytics/db/analytics_db_types.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/analytics_logging_ini.h>
#include <nx/analytics/frame_info.h>
#include <nx/analytics/metadata_logger.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/object_type_dictionary.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/pixelation_settings.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/analytics/object_display_settings.h>
#include <nx/vms/client/core/analytics/object_tracker.h>
#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/analytics/hovered_analytics_object.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/analytics_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/box.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/dummy.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/point.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::common::metadata;
using nx::vms::client::core::Geometry;
using nx::vms::client::core::analytics::ObjectSnapshot;
using nx::vms::client::core::analytics::ObjectTracker;

namespace {

milliseconds toMs(microseconds value)
{
    return duration_cast<milliseconds>(value);
}

bool isValidFigureRectangle(const QRectF& rect)
{
    // Rectangle can be used as a point, so zero width and height are OK.
    return rect.width() >= 0 && rect.height() >= 0;
};

/** Presentation of an analytics object, recalculated when its track gets a new packet. */
struct ObjectAppearance
{
    QColor color;
    QString description;
};

QString objectDebugString(const nx::Uuid& trackId, const ObjectAppearance& appearance)
{
    auto singleLineDescription = appearance.description;
    singleLineDescription.replace('\n', ' ');
    return QString("Object [%1] %2").arg(trackId.toSimpleString()).arg(singleLineDescription);
}

static constexpr auto kShowPointAttributeName = "nx.sys.showAsPoint";

std::optional<QString> attributeValue(
    const Attributes& attributes,
    const QString& name)
{
    const auto it = std::find_if(attributes.cbegin(), attributes.cend(),
        [name](const Attribute& attribute) { return attribute.name == name; });

    return (it == attributes.cend())
        ? std::optional<QString>()
        : it->value;
}

bool isPointFigure(const Attributes& attributes)
{
    return !!attributeValue(attributes, kShowPointAttributeName);
}

figure::FigurePtr figureFromObjectData(const QColor& color,
    const Attributes& attributes,
    const QRectF& boundingRect,
    const QRectF& zoomRect)
{
    if (!isValidFigureRectangle(boundingRect))
        return {};

    auto figure = [&color, &attributes, boundingRect]()
    {
        static constexpr QRectF kWidgetBounds(0.0, 0.0, 1.0, 1.0);
        const auto& rect = Geometry::movedInto(boundingRect, kWidgetBounds);
        const auto& topLeft = rect.topLeft();
        if (isPointFigure(attributes))
            return figure::make<figure::Point>(topLeft, color);

        if (rect.isValid() || rect.isEmpty())
        {
            const figure::Points points{topLeft, rect.bottomRight()};
            return figure::make<figure::Box>(points, color);
        }

        return figure::Dummy::create();
    }();

    if (!zoomRect.isValid())
        return figure;

    if (figure->intersects(zoomRect))
        figure->setSceneRect(zoomRect);
    else
        figure.reset();

    return figure;
}

QString objectDescription(const core::analytics::AttributeList& attributes)
{
    static constexpr qsizetype kExpectedNameAndValueLength = 30;
    QString result;
    result.reserve(attributes.size() * kExpectedNameAndValueLength);

    for (const auto& group: attributes)
    {
        result += group.displayedName;
        result += '\t';
        result += group.displayedValues.join(", ");
        result += '\n';
    }

    result.chop(1); //< Chop trailing `\n`.
    return result;
}

QString approximateDebugTime(microseconds value)
{
    const auto valueMs = toMs(value);
    return nx::utils::timestampToDebugString(valueMs, "hh:mm:ss.zzz")
        + " (" + QString::number(valueMs.count()) +")";
}

} // namespace

class WidgetAnalyticsController::Private: public SystemContextAware
{
public:
    static AnalyticsOverlayWidget::AreaInfo areaInfoFromObject(
        const nx::Uuid& trackId, const ObjectAppearance& appearance);

    Private(WidgetAnalyticsController* parent);

    QnLayoutResourcePtr layoutResource() const;

    const ObjectAppearance& updateAppearance(const ObjectSnapshot& object);

    void updateObjectAreas(milliseconds timestamp);

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    nx::Uuid layoutId;

    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AnalyticsOverlayWidget> analyticsWidget;
    ObjectTracker tracker;
    QHash<nx::Uuid, ObjectAppearance> appearanceByTrackId;

    std::unique_ptr<nx::analytics::MetadataLogger> logger;

    Filter filter;
    milliseconds lastTimestamp{};
    QSet<nx::Uuid> relevantCameraIds;

    const nx::analytics::taxonomy::ObjectTypeDictionary objectTypeDictionary;
};

AnalyticsOverlayWidget::AreaInfo WidgetAnalyticsController::Private::areaInfoFromObject(
    const nx::Uuid& trackId, const ObjectAppearance& appearance)
{
    AnalyticsOverlayWidget::AreaInfo areaInfo;
    areaInfo.id = trackId;
    areaInfo.text = appearance.description;
    areaInfo.hoverText = appearance.description;
    return areaInfo;
}

WidgetAnalyticsController::Private::Private(WidgetAnalyticsController* parent):
    SystemContextAware(parent->systemContext()),
    objectTypeDictionary(systemContext()->analyticsTaxonomyStateWatcher())
{
}

QnLayoutResourcePtr WidgetAnalyticsController::Private::layoutResource() const
{
    return mediaResourceWidget->layoutResource();
}

const ObjectAppearance& WidgetAnalyticsController::Private::updateAppearance(
    const ObjectSnapshot& object)
{
    const auto settings = appContext()->objectDisplaySettings();
    const ObjectMetadata& objectMetadata = object.metadata;

    auto& appearance = appearanceByTrackId[object.trackId];
    appearance.color = settings->objectColor(objectMetadata);

    const auto watcher = systemContext()->analyticsTaxonomyStateWatcher();
    const auto state = NX_ASSERT(watcher) ? watcher->state() : nullptr;
    const auto objectType = state ? state->objectTypeById(objectMetadata.typeId) : nullptr;
    const QString title = objectType ? QString::fromStdString(objectType->name()) : QString();

    const auto visibleAttributes = settings->visibleAttributes(objectMetadata);
    const QString description = objectDescription(systemContext()->analyticsAttributeHelper()->
        preprocessAttributes(QString::fromStdString(objectMetadata.typeId), visibleAttributes));

    appearance.description = title;
    if (!title.isEmpty() && !description.isEmpty())
        appearance.description += "\n";

    appearance.description += description;
    return appearance;
}

void WidgetAnalyticsController::Private::updateObjectAreas(milliseconds timestamp)
{
    for (const auto& object: tracker.objects())
    {
        const auto& appearance = appearanceByTrackId[object.trackId];
        const auto deviceId = mediaResourceWidget->resource()->getId();
        const bool checkBounds = relevantCameraIds.empty()
            || relevantCameraIds.contains(deviceId);

        if (!filter.acceptsMetadata(deviceId,
                object.analyticsEngineId,
                object.metadata,
                objectTypeDictionary,
                checkBounds))
        {
            NX_VERBOSE(
                this, "Object %1 filtered out", objectDebugString(object.trackId, appearance));
            analyticsWidget->removeArea(object.trackId);
            continue;
        }

        auto areaInfo = areaInfoFromObject(object.trackId, appearance);
        auto figureRectangle = object.interpolatedRectangle(timestamp);

        auto pixSetting = systemContext()->globalSettings()->pixelationSettings();
        if (!accessController()->hasGlobalPermissions(GlobalPermission::viewUnredactedVideo)
            && (pixSetting.contains(QString::fromStdString(object.metadata.typeId))))
        {
            const auto watcher = systemContext()->analyticsTaxonomyStateWatcher();
            const auto state = NX_ASSERT(watcher) ? watcher->state() : nullptr;
            const auto objectType =
                state ? state->objectTypeById(object.metadata.typeId) : nullptr;
            const QString title = objectType
                ? QString::fromStdString(objectType->name())
                : QString();
            areaInfo.text = title;
            areaInfo.hoverText = title;
        }
        const auto zoomRect = mediaResourceWidget->zoomRect();
        const auto figure = figureFromObjectData(
            appearance.color, object.metadata.attributes, figureRectangle, zoomRect);
        if (!figure || !figure->isValid())
        {
            NX_VERBOSE(this,
                "Object info has invalid rectangle data or does not fit to the widget: %1",
                objectDebugString(object.trackId, appearance));
            analyticsWidget->removeArea(object.trackId);
            continue;
        }

        const QString debugInfoDescriptor(ini().displayAnalyticsObjectsDebugInfo);
        if (!debugInfoDescriptor.isEmpty())
        {
            const auto addInfoRow =
                [debugInfoDescriptor, &areaInfo, allowAll = debugInfoDescriptor.contains("all")]
                (const QString& key, const QString& label, auto value)
                {
                    if (allowAll || debugInfoDescriptor.contains(key))
                    {
                        const QString rowText =
                            NX_FMT("\n%1\t%2", label, QnLexical::serialized(value));
                        areaInfo.text += rowText;
                        areaInfo.hoverText += rowText;
                    }
                };

            addInfoRow("trackId", "Track ID", object.trackId);
            addInfoRow("delay", "Delay", (timestamp - object.startTimestamp).count());
            addInfoRow("actual_ts", "Timestamp", timestamp.count());
            addInfoRow("actual_rect", "Interpolated", figureRectangle);
            addInfoRow("object_ts", "Original TS", object.startTimestamp.count());
            addInfoRow("object_rect", "Original Rect", object.metadata.boundingBox);
            addInfoRow("future_ts", "Future TS", object.futureRectangleTimestamp.count());
            addInfoRow("future_rect", "Future Rect", object.futureRectangle);
        }

        analyticsWidget->addOrUpdateArea(areaInfo, figure);
    }
}

//-------------------------------------------------------------------------------------------------

WidgetAnalyticsController::WidgetAnalyticsController(QnMediaResourceWidget* mediaResourceWidget):
    SystemContextAware(mediaResourceWidget->systemContext()),
    d(new Private(this))
{
    NX_ASSERT(mediaResourceWidget);
    d->mediaResourceWidget = mediaResourceWidget;

    if (nx::analytics::loggingIni().isLoggingEnabled())
    {
        d->logger = std::make_unique<nx::analytics::MetadataLogger>(
            "widget_analytics_controller_",
            d->mediaResourceWidget->resource()->getId(),
            /*engineId*/ nx::Uuid());
    }
}

WidgetAnalyticsController::~WidgetAnalyticsController()
{
}

// TODO: Use milliseconds in all metadata calculations.
void WidgetAnalyticsController::updateAreas(microseconds timestamp, int channel)
{
    if (!d->metadataProvider || !d->analyticsWidget)
        return;

    if (d->logger)
        d->logger->pushFrameInfo({timestamp});

    // Peek some future metatada packets to prolong existing areas' lifetime at least until the
    // latest track id appearance.
    const std::list<ObjectMetadataPacketPtr> objectMetadataPackets =
        d->metadataProvider->metadataRange(timestamp - ObjectTracker::kMetadataWindowSize,
            timestamp + ObjectTracker::kMetadataWindowSize,
            channel);

    if (d->logger)
    {
        for (const auto& objectPacket: objectMetadataPackets)
            d->logger->pushObjectMetadata(*objectPacket);
    }

    const auto timestampMs = toMs(timestamp);
    const auto result = d->tracker.update(objectMetadataPackets, timestampMs);

    NX_VERBOSE(this,
        "Updating analytics objects; current timestamp %1\n"
        "Found %2 tracks for resource %3",
        approximateDebugTime(timestamp),
        result.updated.size(),
        d->mediaResourceWidget->resource()->getId());

    // Process each object from the actual metadata packets.
    for (const auto& trackId: result.updated)
    {
        const ObjectSnapshot& object = *d->tracker.objects().constFind(trackId);
        const auto& appearance = d->updateAppearance(object);

        NX_VERBOSE(this,
            "Added object\n%1\nat %2\nDuration: %3 - %4",
            objectDebugString(trackId, appearance),
            object.metadata.boundingBox,
            approximateDebugTime(object.startTimestamp),
            approximateDebugTime(object.endTimestamp));
    }

    // Cleanup areas which should not be visible right now.
    for (const auto& trackId: result.removed)
    {
        d->appearanceByTrackId.remove(trackId);
        d->analyticsWidget->removeArea(trackId);
    }

    NX_VERBOSE(
        this, "%1 objects are currently available from RTSP stream", d->tracker.objects().size());

    d->lastTimestamp = timestampMs;
    d->updateObjectAreas(timestampMs);
}

void WidgetAnalyticsController::clearAreas()
{
    for (const auto& object: d->tracker.objects())
        d->analyticsWidget->removeArea(object.trackId);
}

std::optional<HoveredAnalyticsObject> WidgetAnalyticsController::hoveredObject() const
{
    if (!d->analyticsWidget)
        return std::nullopt;

    const auto trackId = d->analyticsWidget->highlightedAreaId();
    if (trackId.isNull())
        return std::nullopt;

    const auto& objects = d->tracker.objects();
    const auto it = objects.constFind(trackId);
    if (it == objects.constEnd())
        return std::nullopt;

    return HoveredAnalyticsObject{it->trackId, it->metadata.typeId, d->lastTimestamp};
}

void WidgetAnalyticsController::setAnalyticsMetadataProvider(
    const core::AbstractAnalyticsMetadataProviderPtr& provider)
{
    d->metadataProvider = provider;
}

void WidgetAnalyticsController::setAnalyticsOverlayWidget(AnalyticsOverlayWidget* widget)
{
    d->analyticsWidget = widget;
}

const WidgetAnalyticsController::Filter& WidgetAnalyticsController::filter() const
{
    return d->filter;
}

void WidgetAnalyticsController::setFilter(const Filter& value)
{
    if (d->filter == value)
        return;

    NX_DEBUG(this, "Update analytics filter to %1", value);
    d->filter = value;
    d->relevantCameraIds.clear();

    for (const auto& id: value.deviceIds)
        d->relevantCameraIds.insert(id);

    if (d->lastTimestamp != 0ms)
        d->updateObjectAreas(d->lastTimestamp);
}

} // namespace nx::vms::client::desktop
