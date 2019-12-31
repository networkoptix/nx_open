#include "widget_analytics_controller.h"

#include <optional>

#include <QtCore/QPointer>

#include <analytics/db/analytics_db_types.h>

#include <common/common_module.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>

#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <nx/utils/log/assert.h>
#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/area_highlight_overlay_widget.h>
#include <nx/vms/client/desktop/analytics/object_display_settings.h>

#include <utils/math/linear_combination.h>

#include <nx/vms/client/desktop/ini.h>

#include <nx/analytics/frame_info.h>
#include <nx/analytics/metadata_logger.h>
#include <nx/analytics/analytics_logging_ini.h>
#include <nx/analytics/object_type_descriptor_manager.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/datetime.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::common::metadata;
using nx::vms::client::core::Geometry;

namespace {

// Peek objects up to 2 seconds in the future. Actual value will be limited by the delay buffer.
static constexpr microseconds kFutureMetadataLength = 2s;

// When metadata duration is not set, object will live for average period between metadata
// packets plus this constant. This constant is needed to avoid areas flickering when average
// metadata period gets a bit lower than a period between two certain metadata packets.
static constexpr microseconds kMinimalObjectDuration = 50ms;

static constexpr QRectF kWidgetBounds(0.0, 0.0, 1.0, 1.0);

struct TrackElement
{
    TrackElement(const QRectF& rectangle, microseconds timestamp):
        rectangle(rectangle), timestamp(timestamp) {}

    QRectF rectangle;
    microseconds timestamp = 0us;

    bool operator<(const TrackElement& other) const
    {
        return timestamp < other.timestamp;
    }
};

/**
 * Track describes behavior of the object sequence, united by the track id. If the metadata packet
 * has fixed duration, first element of the path will be used to determine when the object should be
 * hidden. Otherwise we will use the latest element of the path. Path is never empty!
 * Path is sorted by the timestamps. It is used to smooth rectangle movement through the video
 * frames.
 */
struct Track
{
    ObjectMetadata metadata;
    std::optional<microseconds> endTimestamp;
    std::vector<TrackElement> path; //< Cannot be empty.
};

QString objectDescription(const std::vector<Attribute>& attributes)
{
    QString result;

    for (const auto& attribute: attributes)
    {
        result += attribute.name;
        result += '\t';
        result += attribute.value;
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
    if (!rectangle.isValid())
        return futureRectangle;

    if (!futureRectangle.isValid())
        return rectangle;

    if (futureRectangleTimestamp <= rectangleTimestamp)
        return rectangle;

    const auto factor = (qreal) (timestamp - rectangleTimestamp).count()
        / (qreal) (futureRectangleTimestamp - rectangleTimestamp).count();

    return linearCombine(1 - factor, rectangle, factor, futureRectangle);
}

milliseconds toMs(microseconds value)
{
    return duration_cast<milliseconds>(value);
}

QString approximateDebugTime(microseconds value)
{
    const auto valueMs = toMs(value);
    return nx::utils::timestampToDebugString(valueMs, "hh:mm:ss.zzz")
        + " (" + QString::number(valueMs.count()) +")";
}

} // namespace

class WidgetAnalyticsController::Private: public QObject, public QnCommonModuleAware
{
public:
    struct ObjectInfo
    {
        QnUuid trackId;
        QColor color;
        QRectF rectangle;
        microseconds startTimestamp = 0us;
        microseconds endTimestamp = 0us;
        QString basicDescription;
        QString description;

        QRectF futureRectangle;
        microseconds futureRectangleTimestamp = 0us;

        ObjectMetadata rawData;

        /** Debug representation. */
        QString toString() const;
    };

    static AreaHighlightOverlayWidget::AreaInformation areaInfoFromObject(
        const ObjectInfo& objectInfo);

    Private(WidgetAnalyticsController* parent);

    QnLayoutResourcePtr layoutResource() const;

    ObjectInfo& addOrUpdateObject(const ObjectMetadata& object);
    void removeArea(const ObjectInfo& object);

    /** Remove areas which are not supposed to be displayed on the current frame. */
    void removeObjectAreas(milliseconds timestamp);

    void updateObjectAreas(microseconds timestamp);

    std::vector<Track> fetchTracks(microseconds timestamp, int channel) const;

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    QnUuid layoutId;

    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AreaHighlightOverlayWidget> areaHighlightWidget;

    QHash<QnUuid, ObjectInfo> objectInfoByTrackId;

    std::unique_ptr<nx::analytics::MetadataLogger> logger;

    Filter filter;
    microseconds lastTimestamp{};
    QSet<QnUuid> relevantCameraIds;
};

AreaHighlightOverlayWidget::AreaInformation WidgetAnalyticsController::Private::areaInfoFromObject(
    const WidgetAnalyticsController::Private::ObjectInfo& objectInfo)
{
    AreaHighlightOverlayWidget::AreaInformation areaInfo;
    areaInfo.id = objectInfo.trackId;
    areaInfo.color = objectInfo.color;
    areaInfo.text = objectInfo.basicDescription;
    areaInfo.hoverText = objectInfo.description;
    return areaInfo;
}

QString WidgetAnalyticsController::Private::ObjectInfo::toString() const
{
    auto singleLineDescription = basicDescription;
    singleLineDescription.replace('\n', ' ');
    return QString("Object [%1] %2").arg(trackId.toString()).arg(singleLineDescription);
}

WidgetAnalyticsController::Private::Private(WidgetAnalyticsController* parent):
    QnCommonModuleAware(parent)
{
}

QnLayoutResourcePtr WidgetAnalyticsController::Private::layoutResource() const
{
    return mediaResourceWidget->item()->layout()->resource();
}

WidgetAnalyticsController::Private::ObjectInfo&
    WidgetAnalyticsController::Private::addOrUpdateObject(
        const ObjectMetadata& objectMetadata)
{
    const auto settings = commonModule()->findInstance<ObjectDisplaySettings>();

    auto& objectInfo = objectInfoByTrackId[objectMetadata.trackId];
    objectInfo.trackId = objectMetadata.trackId;
    objectInfo.color = settings->objectColor(objectMetadata);

    objectInfo.rectangle = objectMetadata.boundingBox;

    QString objectTitle;
    const auto descriptor = commonModule()->analyticsObjectTypeDescriptorManager()->descriptor(
        objectMetadata.typeId);
    if (descriptor && !descriptor->name.isEmpty())
        objectTitle = descriptor->name + '\n';

    objectInfo.basicDescription = objectTitle +
        objectDescription(settings->briefAttributes(objectMetadata));
    objectInfo.description = objectTitle +
        objectDescription(settings->visibleAttributes(objectMetadata));

    objectInfo.rawData = objectMetadata;

    return objectInfo;
}

void WidgetAnalyticsController::Private::removeArea(const ObjectInfo& objectInfo)
{
    areaHighlightWidget->removeArea(objectInfo.trackId);
}

void WidgetAnalyticsController::Private::removeObjectAreas(milliseconds timestamp)
{
    for (auto it = objectInfoByTrackId.begin(); it != objectInfoByTrackId.end(); /*-*/)
    {
        if (timestamp < toMs(it->startTimestamp)
            || timestamp > toMs(it->endTimestamp))
        {
            NX_VERBOSE(this, "Cleanup object %1 as it does not fit into timestamp window", *it);
            removeArea(*it);
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
        const bool relevantCamera = relevantCameraIds.empty()
            || relevantCameraIds.contains(mediaResourceWidget->resource()->toResourcePtr()->getId());

        if ((ini().applyCameraFilterToSceneItems && !relevantCamera)
            || !filter.acceptsMetadata(objectInfo.rawData, /*checkBoundingBox*/ relevantCamera))
        {
            NX_VERBOSE(this, "Object %1 filtered out", objectInfo);
            areaHighlightWidget->removeArea(objectInfo.trackId);
            continue;
        }

        auto areaInfo = areaInfoFromObject(objectInfo);
        if (ini().enableObjectMetadataInterpolation)
        {
            areaInfo.rectangle = interpolatedRectangle(
                objectInfo.rectangle,
                objectInfo.startTimestamp,
                objectInfo.futureRectangle,
                objectInfo.futureRectangleTimestamp,
                timestamp);
        }
        else
        {
            areaInfo.rectangle = objectInfo.rectangle;
        }

        if (!areaInfo.rectangle.isValid())
        {
            NX_VERBOSE(this, "Object info has invalid rectangle data: %1", objectInfo);
            areaHighlightWidget->removeArea(objectInfo.trackId);
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
            if (ini().enableObjectMetadataInterpolation)
                addInfoRow("actual_rect", "Interpolated", areaInfo.rectangle);
            addInfoRow("object_ts", "Original TS", objectInfo.startTimestamp.count());
            addInfoRow("object_rect", "Original Rect", objectInfo.rectangle);
            addInfoRow("future_ts", "Future TS", objectInfo.futureRectangleTimestamp.count());
            addInfoRow("future_rect", "Future Rect", objectInfo.futureRectangle);
        }

        areaInfo.rectangle = Geometry::movedInto(areaInfo.rectangle, kWidgetBounds);

        // If the current widget is zoom window itself, transpose object area correspondingly.
        if (mediaResourceWidget->isZoomWindow())
        {
            areaInfo.rectangle = Geometry::toSubRect(mediaResourceWidget->zoomRect(),
                areaInfo.rectangle);

            // Remove areas that are not fit into current zoom window.
            areaInfo.rectangle = Geometry::intersection(areaInfo.rectangle, kWidgetBounds);
            if (areaInfo.rectangle.isEmpty())
            {
                areaHighlightWidget->removeArea(areaInfo.id);
                continue;
            }
        }

        areaHighlightWidget->addOrUpdateArea(areaInfo);
    }
}

std::vector<Track> WidgetAnalyticsController::Private::fetchTracks(
    microseconds timestamp, int channel) const
{
    if (logger)
        logger->pushFrameInfo({timestamp});

    // Peek some future metatada packets to prolong existing areas' lifetime at least until the
    // latest track id appearance.
    QList<ObjectMetadataPacketPtr> objectMetadataPackets = metadataProvider->metadataRange(
        timestamp,
        timestamp + kFutureMetadataLength,
        channel);

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
            if (objectMetadata.bestShot)
                continue; //< Skip specialized best shot records.

            Track& track = objectTrackByTrackId[objectMetadata.trackId];
            if (track.path.empty())
            {
                track.metadata = objectMetadata;
                if (packetHasDuration)
                    track.endTimestamp = timestamp + microseconds(objectPacket->durationUs);
            }

            track.path.emplace_back(objectMetadata.boundingBox, timestamp);
            NX_ASSERT(std::is_sorted(track.path.cbegin(), track.path.cend()));
        }
    }

    // TODO: Use milliseconds in all metadata calculations.
    const auto timestampMs = toMs(timestamp);

    std::vector<Track> result;
    for (const auto& track: objectTrackByTrackId)
    {
        if (toMs(track.path.cbegin()->timestamp) <= timestampMs)
            result.push_back(track);
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

WidgetAnalyticsController::WidgetAnalyticsController(QnMediaResourceWidget* mediaResourceWidget):
    base_type(mediaResourceWidget),
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
    if (!d->metadataProvider || !d->areaHighlightWidget)
        return;

    const std::vector<Track> tracks = d->fetchTracks(timestamp, channel);

    NX_VERBOSE(this,
        "Updating analytics objects; current timestamp %1\n"
        "Found %2 tracks for resource %3",
        approximateDebugTime(timestamp),
        tracks.size(),
        d->mediaResourceWidget->resource()->toResourcePtr()->getId());

    // Process each object from the actual metadata packets.
    for (const auto& track: tracks)
    {
        auto& objectInfo = d->addOrUpdateObject(track.metadata);
        objectInfo.startTimestamp = track.path.front().timestamp;
        objectInfo.endTimestamp = track.endTimestamp
            ? *track.endTimestamp
            : track.path.back().timestamp + kMinimalObjectDuration;

        // Make forecast for the rectangle movement to smooth it between analytics packets.
        if (track.path.size() > 1)
        {
            const auto& nextElement = track.path[1];
            objectInfo.futureRectangleTimestamp = nextElement.timestamp;
            objectInfo.futureRectangle = nextElement.rectangle;
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
        d->removeArea(objectInfo);
}

void WidgetAnalyticsController::setAnalyticsMetadataProvider(
    const core::AbstractAnalyticsMetadataProviderPtr& provider)
{
    d->metadataProvider = provider;
}

void WidgetAnalyticsController::setAreaHighlightOverlayWidget(AreaHighlightOverlayWidget* widget)
{
    d->areaHighlightWidget = widget;
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
