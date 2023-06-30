// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "widget_analytics_controller.h"

#include <optional>

#include <QtCore/QPointer>

#include <analytics/common/object_metadata.h>
#include <analytics/db/analytics_db_types.h>
#include <client/client_module.h>
#include <common/common_module.h>
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
#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/analytics/object_display_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/analytics_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/box.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/dummy.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/point.h>
#include <nx/vms/common/system_context.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/math/linear_combination.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::common::metadata;
using nx::vms::client::core::Geometry;

namespace {

// Peek objects up to 2 seconds in the future and into the past.
static constexpr microseconds kMetadataWindowSize = 2s;

// When metadata duration is not set, object will live for average period between metadata
// packets plus this constant. This constant is needed to avoid areas flickering when average
// metadata period gets a bit lower than a period between two certain metadata packets.
static constexpr microseconds kMinimalObjectDuration = 200ms;

milliseconds toMs(microseconds value)
{
    return duration_cast<milliseconds>(value);
}

bool isValidFigureRectangle(const QRectF& rect)
{
    // Rectangle can be used as a point, so zero width and height are OK.
    return rect.width() >= 0 && rect.height() >= 0;
};

struct ObjectInfo
{
    QnUuid trackId;
    QColor color;
    QRectF rectangle;
    microseconds startTimestamp = 0us;
    microseconds endTimestamp = 0us;
    QString description;

    QRectF futureRectangle;
    microseconds futureRectangleTimestamp = 0us;

    ObjectMetadata rawData;

    /** Debug representation. */
    QString toString() const;
};

QString ObjectInfo::toString() const
{
    auto singleLineDescription = description;
    singleLineDescription.replace('\n', ' ');
    return QString("Object [%1] %2").arg(trackId.toString()).arg(singleLineDescription);
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

figure::FigurePtr figureFromObjectData(
    const ObjectInfo& info,
    const QRectF& boundingRect,
    const QRectF& zoomRect)
{
    if (!isValidFigureRectangle(boundingRect))
        return {};

    auto figure =
        [info, boundingRect]()
        {
            static constexpr QRectF kWidgetBounds(0.0, 0.0, 1.0, 1.0);
            const auto& rect = Geometry::movedInto(boundingRect, kWidgetBounds);
            const auto& attributes = info.rawData.attributes;
            const auto& topLeft = rect.topLeft();
            if (isPointFigure(attributes))
                return figure::make<figure::Point>(topLeft, info.color);

            if (rect.isValid() || rect.isEmpty())
            {
                const figure::Points points{topLeft, rect.bottomRight()};
                return figure::make<figure::Box>(points, info.color);
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

/**
 * Path element inside Track. When constructing this structure make sure that the provided
 * ObjectMetadata always exists for the whole lifetime of the Track.
 */
struct TrackElement
{
    TrackElement(const ObjectMetadata* metadata, microseconds timestamp):
        metadata(metadata), timestamp(timestamp) {}

    const ObjectMetadata* metadata;
    microseconds timestamp = 0us;

    bool operator<(const TrackElement& other) const
    {
        return timestamp < other.timestamp;
    }

    bool operator<(const microseconds& other) const
    {
        return timestamp < other;
    }
};

/**
 * Track describes behavior of the object sequence, united by the track id. Path is never empty!
 * Path is sorted by the timestamps. It is used to smooth rectangle movement through the video
 * frames.
 */
struct Track
{
    std::optional<microseconds> minimalDuration;
    std::vector<TrackElement> path; //< Cannot be empty.

    microseconds startTimestamp() const
    {
         return path.front().timestamp;
    }

    /**
     * If we have minimal duration in the first track packet, we will used it to determine when to
     * hide the frame. Otherwise we will use approximate value. Anyway, frame must be displayed all
     * the time we are receiving the track packets.
     */
    microseconds endTimestamp() const
    {
        const auto approximatedTimestamp = path.back().timestamp + kMinimalObjectDuration;
        return minimalDuration
            ? std::max(startTimestamp() + *minimalDuration, approximatedTimestamp)
            : approximatedTimestamp;
    }

    /**
     * Trim track by start time, leaving the only one path element, located before or on the
     * selected time.
     */
    void trimByStartTime(milliseconds startTime)
    {
        auto iter = std::lower_bound(path.begin(), path.end(), startTime);
        if (iter == path.end() || (iter != path.begin() && toMs(iter->timestamp) > startTime))
            --iter;
        path.erase(path.begin(), iter);
        NX_ASSERT(!path.empty());
    }
};

QString objectDescription(const core::analytics::AttributeList& attributes)
{
    QString result;

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

QRectF interpolatedRectangle(
    const QRectF& rectangle,
    microseconds rectangleTimestamp,
    const QRectF& futureRectangle,
    microseconds futureRectangleTimestamp,
    microseconds timestamp)
{
    if (!isValidFigureRectangle(rectangle))
        return futureRectangle;

    if (!isValidFigureRectangle(futureRectangle))
        return rectangle;

    if (futureRectangleTimestamp <= rectangleTimestamp)
        return rectangle;

    const auto factor = (qreal) (timestamp - rectangleTimestamp).count()
        / (qreal) (futureRectangleTimestamp - rectangleTimestamp).count();

    return linearCombine(1 - factor, rectangle, factor, futureRectangle);
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
        const ObjectInfo& objectInfo);

    Private(WidgetAnalyticsController* parent);

    QnLayoutResourcePtr layoutResource() const;

    ObjectInfo& addOrUpdateObject(const ObjectMetadata& object);

    /** Remove areas which are not supposed to be displayed on the current frame. */
    void removeObjectAreas(milliseconds timestamp);

    void updateObjectAreas(microseconds timestamp);

    std::vector<Track> fetchTracks(
        const QList<ObjectMetadataPacketPtr>& objectMetadataPackets,
        microseconds timestamp) const;

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    QnUuid layoutId;

    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AnalyticsOverlayWidget> analyticsWidget;
    QHash<QnUuid, ObjectInfo> objectInfoByTrackId;

    std::unique_ptr<nx::analytics::MetadataLogger> logger;

    Filter filter;
    microseconds lastTimestamp{};
    QSet<QnUuid> relevantCameraIds;

    const nx::analytics::taxonomy::ObjectTypeDictionary objectTypeDictionary;
};

AnalyticsOverlayWidget::AreaInfo WidgetAnalyticsController::Private::areaInfoFromObject(
    const ObjectInfo& objectInfo)
{
    AnalyticsOverlayWidget::AreaInfo areaInfo;
    areaInfo.id = objectInfo.trackId;
    areaInfo.text = objectInfo.description;
    areaInfo.hoverText = objectInfo.description;
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

ObjectInfo& WidgetAnalyticsController::Private::addOrUpdateObject(
    const ObjectMetadata& objectMetadata)
{
    const auto settings = appContext()->objectDisplaySettings();

    auto& objectInfo = objectInfoByTrackId[objectMetadata.trackId];
    objectInfo.trackId = objectMetadata.trackId;
    objectInfo.color = settings->objectColor(objectMetadata);

    objectInfo.rectangle = objectMetadata.boundingBox;

    const auto watcher = systemContext()->analyticsTaxonomyStateWatcher();
    const auto state = NX_ASSERT(watcher) ? watcher->state() : nullptr;
    const auto objectType = state ? state->objectTypeById(objectMetadata.typeId) : nullptr;
    const QString title = objectType ? objectType->name() : QString();

    const auto visibleAttributes = settings->visibleAttributes(objectMetadata);
    const Attributes sortedAttributes(visibleAttributes.cbegin(), visibleAttributes.cend());
    const QString description = objectDescription(systemContext()->analyticsAttributeHelper()->
        preprocessAttributes(objectMetadata.typeId, sortedAttributes));

    objectInfo.description = title;
    if (!title.isEmpty() && !description.isEmpty())
        objectInfo.description += "\n";

    objectInfo.description += description;
    objectInfo.rawData = objectMetadata;
    return objectInfo;
}

void WidgetAnalyticsController::Private::removeObjectAreas(milliseconds timestamp)
{
    for (auto it = objectInfoByTrackId.begin(); it != objectInfoByTrackId.end(); /*-*/)
    {
        if (timestamp < toMs(it->startTimestamp)
            || timestamp > toMs(it->endTimestamp))
        {
            NX_VERBOSE(this, "Cleanup object %1 as it does not fit into timestamp window", *it);
            analyticsWidget->removeArea(it->trackId);
            it = objectInfoByTrackId.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void WidgetAnalyticsController::Private::updateObjectAreas(microseconds timestamp)
{
    for (const auto& objectInfo: objectInfoByTrackId)
    {
        const auto deviceId = mediaResourceWidget->resource()->toResourcePtr()->getId();
        const bool checkBounds = relevantCameraIds.empty()
            || relevantCameraIds.contains(deviceId);

        if (!filter.acceptsMetadata(deviceId, objectInfo.rawData, objectTypeDictionary,checkBounds))
        {
            NX_VERBOSE(this, "Object %1 filtered out", objectInfo);
            analyticsWidget->removeArea(objectInfo.trackId);
            continue;
        }

        auto areaInfo = areaInfoFromObject(objectInfo);
        auto figureRectangle = interpolatedRectangle(
            objectInfo.rectangle,
            objectInfo.startTimestamp,
            objectInfo.futureRectangle,
            objectInfo.futureRectangleTimestamp,
            timestamp);

        const auto zoomRect = mediaResourceWidget->zoomRect();
        const auto figure = figureFromObjectData(objectInfo, figureRectangle, zoomRect);
        if (figure && figure->isValid())
        {
            analyticsWidget->addOrUpdateArea(areaInfo, figure);
        }
        else
        {
            NX_VERBOSE(this,
                "Object info has invalid rectangle data or does not fit to the widget: %1",
                objectInfo);
            analyticsWidget->removeArea(objectInfo.trackId);
            continue;
        }

        QString debugInfoDescriptor(ini().displayAnalyticsObjectsDebugInfo);
        if (!debugInfoDescriptor.isEmpty())
        {
            const auto addInfoRow =
                [debugInfoDescriptor, &areaInfo, allowAll = debugInfoDescriptor.contains("all")]
                (const QString& key, const QString& label, auto value)
                {
                    if (allowAll || debugInfoDescriptor.contains(key))
                    {
                        areaInfo.text += '\n';
                        areaInfo.text += label;
                        areaInfo.text += '\t';
                        areaInfo.text += QnLexical::serialized(value);
                    }
                };

            addInfoRow("trackId", "Track ID", objectInfo.trackId);
            addInfoRow("delay", "Delay", (timestamp - objectInfo.startTimestamp).count() / 1000);
            addInfoRow("actual_ts", "Timestamp", timestamp.count());
            addInfoRow("actual_rect", "Interpolated", figureRectangle);
            addInfoRow("object_ts", "Original TS", objectInfo.startTimestamp.count());
            addInfoRow("object_rect", "Original Rect", objectInfo.rectangle);
            addInfoRow("future_ts", "Future TS", objectInfo.futureRectangleTimestamp.count());
            addInfoRow("future_rect", "Future Rect", objectInfo.futureRectangle);
        }
    }
}

std::vector<Track> WidgetAnalyticsController::Private::fetchTracks(
    const QList<ObjectMetadataPacketPtr>& objectMetadataPackets,
    microseconds timestamp) const
{
    // Left only objects which are to be displayed right now, store tracks for others.
    QHash<QnUuid, Track> objectTrackByTrackId;
    for (const auto& objectPacket: objectMetadataPackets)
    {
        if (logger)
            logger->pushObjectMetadata(*objectPacket);

        const auto timestamp = microseconds(objectPacket->timestampUs);
        const bool packetHasDuration = objectPacket->durationUs > 0;

        // Each track will start from the actual object and be sorted by timestamps.
        for (const auto& objectMetadata: objectPacket->objectMetadataList)
        {
            if (objectMetadata.isBestShot())
                continue; //< Skip specialized best shot records.

            Track& track = objectTrackByTrackId[objectMetadata.trackId];
            if (packetHasDuration)
                track.minimalDuration = microseconds(objectPacket->durationUs);

            track.path.emplace_back(&objectMetadata, timestamp);
            NX_ASSERT_HEAVY_CONDITION(std::is_sorted(track.path.cbegin(), track.path.cend()));
        }
    }

    // TODO: Use milliseconds in all metadata calculations.
    const auto timestampMs = toMs(timestamp);

    std::vector<Track> result;
    for (auto track: objectTrackByTrackId)
    {
        // Skip tracks which end before the current frame.
        if (toMs(track.endTimestamp()) <= timestampMs)
            continue;

        track.trimByStartTime(timestampMs);

        if (toMs(track.path.cbegin()->timestamp) <= timestampMs)
            result.push_back(track);
    }
    return result;
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
            d->mediaResourceWidget->resource()->toResourcePtr()->getId(),
            /*engineId*/ QnUuid());
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
    const QList<ObjectMetadataPacketPtr> objectMetadataPackets = d->metadataProvider->metadataRange(
        timestamp - kMetadataWindowSize,
        timestamp + kMetadataWindowSize,
        channel);

    // Each Track contains a raw pointer inside an ObjectMetadataPacketPtr, so the lifetime of
    // objectMetadataPackets should always be greater than the lifetime of tracks.
    const std::vector<Track> tracks = d->fetchTracks(objectMetadataPackets, timestamp);

    NX_VERBOSE(this,
        "Updating analytics objects; current timestamp %1\n"
        "Found %2 tracks for resource %3",
        approximateDebugTime(timestamp),
        tracks.size(),
        d->mediaResourceWidget->resource()->toResourcePtr()->getId());

    // Process each object from the actual metadata packets.
    for (const auto& track: tracks)
    {
        auto& objectInfo = d->addOrUpdateObject(*track.path[0].metadata);
        objectInfo.startTimestamp = track.startTimestamp();
        objectInfo.endTimestamp = track.endTimestamp();

        // Make forecast for the rectangle movement to smooth it between analytics packets.
        if (track.path.size() > 1)
        {
            const auto& nextElement = track.path[1];
            objectInfo.futureRectangleTimestamp = nextElement.timestamp;
            objectInfo.futureRectangle = nextElement.metadata->boundingBox;
        }
        else
        {
            objectInfo.futureRectangleTimestamp = objectInfo.startTimestamp;
        }

        NX_VERBOSE(this, "Added object\n%1\nat %2\nDuration: %3 - %4",
            objectInfo,
            objectInfo.rectangle,
            approximateDebugTime(objectInfo.startTimestamp),
            approximateDebugTime(objectInfo.endTimestamp));
    }

    // Cleanup areas which should not be visible right now.
    d->removeObjectAreas(toMs(timestamp));

    NX_VERBOSE(this,
        "%1 objects are currently available from RTSP stream",
        d->objectInfoByTrackId.size());

    d->lastTimestamp = timestamp;
    d->updateObjectAreas(timestamp);
}

void WidgetAnalyticsController::clearAreas()
{
    for (auto& objectInfo: d->objectInfoByTrackId)
        d->analyticsWidget->removeArea(objectInfo.trackId);
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

    if (d->lastTimestamp != 0us)
        d->updateObjectAreas(d->lastTimestamp);
}

} // namespace nx::vms::client::desktop
