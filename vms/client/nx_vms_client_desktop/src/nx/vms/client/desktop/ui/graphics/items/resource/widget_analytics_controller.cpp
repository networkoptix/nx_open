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

static constexpr microseconds kFutureMetadataLength = 2s;
static constexpr int kMaxFutureMetadataPackets = 4;
static constexpr QRectF kWidgetBounds(0.0, 0.0, 1.0, 1.0);

QString objectDescription(const std::vector<Attribute>& attributes)
{
    QString result;

    for (const auto& attribute: attributes)
    {
        result += attribute.name;
        result += L'\t';
        result += attribute.value;
        result += L'\n';
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
    if (futureRectangleTimestamp <= rectangleTimestamp)
        return rectangle;

    const auto factor = (qreal) (timestamp - rectangleTimestamp).count()
        / (qreal) (futureRectangleTimestamp - rectangleTimestamp).count();

    return linearCombine(1 - factor, rectangle, factor, futureRectangle);
}

/**
 * Average time between metadata packets in the RTSP stream.
 */
microseconds calculateAverageMetadataPeriod(
    const QList<ObjectMetadataPacketPtr>& metadataList)
{
    if (metadataList.size() <= 1)
        return 0us;

    microseconds result = 0us;

    for (auto it = metadataList.begin() + 1; it != metadataList.end(); ++it)
        result += microseconds((*it)->timestampUs - (*(it - 1))->timestampUs);

    result /= metadataList.size() - 1;

    return result;
}

std::optional<std::pair<ObjectMetadataPacketPtr, ObjectMetadata>> findObjectMetadataByTrackId(
    const QList<ObjectMetadataPacketPtr>& metadataList,
    const QnUuid& trackId)
{
    for (const auto& packet: metadataList)
    {
        for (const auto& objectMetadata: packet->objectMetadataList)
        {
            if (objectMetadata.trackId == trackId)
                return {{packet, objectMetadata}};
        }
    }
    return std::nullopt;
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
        QnUuid id;
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

    void updateObjectAreas(microseconds timestamp);

    microseconds metadataEndTimestamp(const ObjectMetadataPacketPtr& metadata) const;

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    QnUuid layoutId;

    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AreaHighlightOverlayWidget> areaHighlightWidget;

    QHash<QnUuid, ObjectInfo> objectInfoById;

    microseconds averageMetadataPeriod = 1s;
    std::unique_ptr<nx::analytics::MetadataLogger> logger;

    Filter filter;
    microseconds lastTimestamp{};
    QSet<QnUuid> relevantCameraIds;
};

AreaHighlightOverlayWidget::AreaInformation WidgetAnalyticsController::Private::areaInfoFromObject(
    const WidgetAnalyticsController::Private::ObjectInfo& objectInfo)
{
    AreaHighlightOverlayWidget::AreaInformation areaInfo;
    areaInfo.id = objectInfo.id;
    areaInfo.color = objectInfo.color;
    areaInfo.text = objectInfo.basicDescription;
    areaInfo.hoverText = objectInfo.description;
    return areaInfo;
}

QString WidgetAnalyticsController::Private::ObjectInfo::toString() const
{
    auto singleLineDescription = basicDescription;
    singleLineDescription.replace('\n', ' ');
    return QString("Object [%1] %2").arg(id.toString()).arg(singleLineDescription);
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

    auto& objectInfo = objectInfoById[objectMetadata.trackId];
    objectInfo.id = objectMetadata.trackId;
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
    areaHighlightWidget->removeArea(objectInfo.id);
}

void WidgetAnalyticsController::Private::updateObjectAreas(microseconds timestamp)
{
    for (const auto& objectInfo: objectInfoById)
    {
        const bool relevantCamera = relevantCameraIds.empty()
            || relevantCameraIds.contains(mediaResourceWidget->resource()->toResourcePtr()->getId());

        if ((ini().applyCameraFilterToSceneItems && !relevantCamera)
            || !filter.acceptsMetadata(objectInfo.rawData, /*checkBoundingBox*/ relevantCamera))
        {
            NX_VERBOSE(this, "Object %1 filtered out", objectInfo);
            areaHighlightWidget->removeArea(objectInfo.id);
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

            addInfoRow("id", "ID", objectInfo.id);
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

microseconds WidgetAnalyticsController::Private::metadataEndTimestamp(
    const ObjectMetadataPacketPtr& metadata) const
{
    // When metadata duration is not set, object will live for average period between metadata
    // packets plus this constant. This constant is needed to avoid areas flickering when average
    // metadata period gets a bit lower than a period between two certain metadata packets.
    static constexpr auto kAdditionalTimeToLive = 50ms;

    const auto minimalObjectDuration = averageMetadataPeriod + kAdditionalTimeToLive;
    const auto actualObjectDuration = metadata->durationUs > 0
        ? microseconds(metadata->durationUs)
        : minimalObjectDuration;
    return microseconds(metadata->timestampUs) + actualObjectDuration;
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

void WidgetAnalyticsController::updateAreas(microseconds timestamp, int channel)
{
    if (!d->metadataProvider || !d->areaHighlightWidget)
        return;

    if (d->logger)
        d->logger->pushFrameInfo({timestamp});

    // TODO: Use milliseconds in all metadata calculations.
    const auto timestampMs = toMs(timestamp);

    // IMPORTANT: Current implementation is intended to work well only if exactly one analytics
    // plugin is enabled on the device. If several plugins are enabled, future metadata packets
    // preview can contain only one plugin data, and this will lead to another plugin's areas to be
    // removed.

    // Peek some future metatada packets to prolong existing areas' lifetime at least until the
    // latest track id appearance.
    auto objectMetadataPackets = d->metadataProvider->metadataRange(
        timestamp,
        timestamp + kFutureMetadataLength,
        channel,
        kMaxFutureMetadataPackets);

    // We are approximating the average time between metadata packets. Will work well for the single
    // analytics plugin only.
    if (microseconds period = calculateAverageMetadataPeriod(objectMetadataPackets); period > 0us)
        d->averageMetadataPeriod = period;

    NX_VERBOSE(this,
        "Updating analytics objects; current timestamp %1\n"
        "Size of metadata list for resource %2: %3\n"
        "Average request period %4",
        approximateDebugTime(timestamp),
        d->mediaResourceWidget->resource()->toResourcePtr()->getId(),
        objectMetadataPackets.size(),
        toMs(d->averageMetadataPeriod));

    // If the plugin provides duration, approximate prolongation is not needed.
    bool currentMetadataHasDuration = false;

    if (!objectMetadataPackets.isEmpty())
    {
        const auto& metadata = objectMetadataPackets.first();
        if (toMs(microseconds(metadata->timestampUs)) <= toMs(timestamp))
        {
            if (d->logger)
                d->logger->pushObjectMetadata(*metadata);

            currentMetadataHasDuration = metadata->durationUs > 0;

            for (const auto& objectMetadata: metadata->objectMetadataList)
            {
                if (objectMetadata.bestShot)
                    continue; //< Skip specialized best shot records.

                auto& objectInfo = d->addOrUpdateObject(objectMetadata);
                objectInfo.startTimestamp = microseconds(metadata->timestampUs);
                objectInfo.endTimestamp = d->metadataEndTimestamp(metadata);
                objectInfo.futureRectangleTimestamp = objectInfo.startTimestamp;
                NX_VERBOSE(this, "Added object\n%1\nat %2\nDuration: %3 - %4",
                    objectInfo,
                    objectInfo.rectangle,
                    approximateDebugTime(objectInfo.startTimestamp),
                    approximateDebugTime(objectInfo.endTimestamp));
            }

            objectMetadataPackets.removeFirst();
        }
    }

    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); /*no increment*/)
    {
        const auto futureMetadata = findObjectMetadataByTrackId(objectMetadataPackets, it.key());

        // Next packet for the same track id is found.
        if (futureMetadata)
        {
            const auto& [packet, objectMetadata] = *futureMetadata;

            // Prolong area existance if needed.
            if (!currentMetadataHasDuration)
            {
                it->endTimestamp = std::max(it->endTimestamp, d->metadataEndTimestamp(packet));
                NX_VERBOSE(this, "Prolonged object %1 to %2 based on the future object",
                    *it, approximateDebugTime(it->endTimestamp));
            }

            // Update geometry approximation information.
            it->futureRectangle = objectMetadata.boundingBox;
            it->futureRectangleTimestamp = microseconds(packet->timestampUs);
        }

        // Cleanup areas which should not be visible right now.
        if (timestampMs < toMs(it->startTimestamp)
            || timestampMs > toMs(it->endTimestamp))
        {
            NX_VERBOSE(this, "Cleanup object %1 as it does not fit into timestamp window", *it);
            d->removeArea(*it);
            it = d->objectInfoById.erase(it);
        }
        else
        {
            ++it;
        }
    }

    NX_VERBOSE(this, "%1 objects are currently available from RTSP stream", d->objectInfoById.size());

    d->lastTimestamp = timestamp;
    d->updateObjectAreas(timestamp);
}

void WidgetAnalyticsController::clearAreas()
{
    for (auto& objectInfo: d->objectInfoById)
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
