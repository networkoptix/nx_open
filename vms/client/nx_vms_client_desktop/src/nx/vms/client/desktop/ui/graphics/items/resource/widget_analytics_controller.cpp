#include "widget_analytics_controller.h"

#include <QtCore/QPointer>

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

microseconds calculateAverageMetadataPeriod(
    const QList<DetectionMetadataPacketPtr>& metadataList)
{
    if (metadataList.size() <= 1)
        return 0us;

    microseconds result = 0us;

    for (auto it = metadataList.begin() + 1; it != metadataList.end(); ++it)
        result += microseconds((*it)->timestampUsec - (*(it - 1))->timestampUsec);

    result /= metadataList.size() - 1;

    return result;
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
    };

    static AreaHighlightOverlayWidget::AreaInformation areaInfoFromObject(
        const ObjectInfo& objectInfo);

    Private(WidgetAnalyticsController* parent);

    QnLayoutResourcePtr layoutResource() const;

    ObjectInfo& addOrUpdateObject(const DetectedObject& object);
    void removeArea(ObjectInfo& object);

    void updateObjectAreas(microseconds timestamp);

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    QnUuid layoutId;

    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AreaHighlightOverlayWidget> areaHighlightWidget;

    QHash<QnUuid, ObjectInfo> objectInfoById;

    microseconds averageMetadataPeriod = 1s;
    std::unique_ptr<nx::analytics::MetadataLogger> logger;
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
        const DetectedObject& object)
{
    const auto settings = commonModule()->findInstance<ObjectDisplaySettings>();

    auto& objectInfo = objectInfoById[object.objectId];
    objectInfo.id = object.objectId;
    objectInfo.color = settings->objectColor(object);

    objectInfo.rectangle = object.boundingBox;
    objectInfo.basicDescription = objectDescription(settings->briefAttributes(object));
    objectInfo.description = objectDescription(settings->visibleAttributes(object));

    return objectInfo;
}

void WidgetAnalyticsController::Private::removeArea(ObjectInfo& object)
{
    areaHighlightWidget->removeArea(object.id);
}

void WidgetAnalyticsController::Private::updateObjectAreas(microseconds timestamp)
{
    for (const auto& objectInfo: objectInfoById)
    {
        auto areaInfo = areaInfoFromObject(objectInfo);

        if (ini().displayAnalyticsDelay)
        {
            areaInfo.text += QString("\nDelay\t%1").arg(
                (timestamp - objectInfo.startTimestamp).count() / 1000);
        }

        if (ini().enableDetectedObjectsInterpolation)
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
    // When metadata duration is not set, object will live for average period between metadata
    // packets plus this constant. This constant is needed to avoid areas flickering when average
    // metadata period gets a bit lower than a period between two certain metadata packets.
    static constexpr auto kAdditionalTimeToLive = 50ms;

    if (!d->metadataProvider || !d->areaHighlightWidget)
        return;

    if (d->logger)
        d->logger->pushFrameInfo(std::make_unique<nx::analytics::FrameInfo>(timestamp));

    auto metadataList = d->metadataProvider->metadataRange(
        timestamp,
        timestamp + kFutureMetadataLength,
        channel,
        kMaxFutureMetadataPackets);

    if (microseconds period = calculateAverageMetadataPeriod(metadataList); period > 0us)
        d->averageMetadataPeriod = period;

    NX_VERBOSE(this, "Size of metadata list for resource %1: %2",
        d->mediaResourceWidget->resource()->toResourcePtr()->getId(),
        metadataList.size());

    if (!metadataList.isEmpty())
    {
        if (const auto& metadata = metadataList.first();
            metadata->timestampUsec <= timestamp.count())
        {
            if (d->logger)
                d->logger->pushObjectMetadata(*metadata);

            for (const auto& object: metadata->objects)
            {
                if (object.bestShot)
                    continue; //< Skip specialized best shot records.

                auto& objectInfo = d->addOrUpdateObject(object);
                objectInfo.startTimestamp = microseconds(metadata->timestampUsec);
                objectInfo.endTimestamp = metadata->durationUsec > 0
                    ? objectInfo.startTimestamp + microseconds(metadata->durationUsec)
                    : objectInfo.startTimestamp + d->averageMetadataPeriod + kAdditionalTimeToLive;
                objectInfo.futureRectangleTimestamp = objectInfo.startTimestamp;
            }

            metadataList.removeFirst();
        }
    }

    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); /*no increment*/)
    {
        if (timestamp < it->startTimestamp || timestamp > it->endTimestamp)
        {
            d->removeArea(*it);
            it = d->objectInfoById.erase(it);
            continue;
        }

        [&]() // Find and update future rectangle for the object.
        {
            for (const auto& metadata: metadataList)
            {
                for (const auto& object: metadata->objects)
                {
                    if (object.objectId == it.key())
                    {
                        it->futureRectangle = object.boundingBox;
                        it->futureRectangleTimestamp = microseconds(metadata->timestampUsec);
                        return;
                    }
                }
            }
        }();

        ++it;
    }

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

} // namespace nx::vms::client::desktop
