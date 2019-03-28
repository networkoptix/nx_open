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

const QString kWidgetAnalyticsControllerFrameLogPattern(
    "Displaying frame in the client. Frame timestamp (ms): {:frame_timestamp}, "
    "current time (ms): {:current_time}, "
    "diff from prev frame (ms): {:frame_time_difference_from_previous_frame}, "
    "diff from current time (ms): {:frame_time_difference_from_current_time}");

const QString kWidgetAnalyticsControllerMetadataLogPattern(
    "Displaying metadata in the client. Metadata timestamp (ms): {:metadata_timestamp}, "
    "current time (ms): {:current_time}, "
    "diff from prev metadata (ms): {:metadata_time_differnce_from_previous_metadata}, "
    "diff from current time (ms): {:metadata_time_difference_from_current_time}, "
    "diff from the last frame (ms): {:metadata_time_difference_from_last_frame}");

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

QnLayoutItemData findItemForObject(const QnLayoutResourcePtr& layout, const QnUuid& objectId)
{
    if (objectId.isNull())
        return {};

    for (const auto& item: layout->getItems())
    {
        const auto id = qnResourceRuntimeDataManager->layoutItemData(
            item.uuid, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

        if (id == objectId)
            return item;
    }

    return {};
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
        QnUuid zoomWindowItemUuid;
        QRectF rectangle;
        microseconds startTimestamp = 0us;
        microseconds endTimestamp = 0us;
        QString basicDescription;
        QString description;

        QRectF futureRectangle;
        microseconds futureRectangleTimestamp = 0us;
    };

    static ObjectInfo objectInfoFromLayoutItem(const QnLayoutItemData& item);
    static QRectF zoomWindowRectangle(const QRectF& objectRect);
    static AreaHighlightOverlayWidget::AreaInformation areaInfoFromObject(
        const ObjectInfo& objectInfo);

    Private(WidgetAnalyticsController* parent);

    void findExistingItems();

    void updateZoomWindowForArea(const QnUuid& areaId);

    QnLayoutResourcePtr layoutResource() const;

    ObjectInfo& addOrUpdateObject(const DetectedObject& object);
    void removeAreaAndZoomWindow(ObjectInfo& object);

    void updateObjectAreas(microseconds timestamp);

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    QnUuid layoutId;

    // This ID is set when the controller operates on a zoom window widget. The controller
    // highlights the object bounding box inside the zoom window.
    QnUuid zoomWindowObjectId;

    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AreaHighlightOverlayWidget> areaHighlightWidget;

    QHash<QnUuid, ObjectInfo> objectInfoById;

    microseconds averageMetadataPeriod = 1s;
    std::unique_ptr<nx::analytics::MetadataLogger> logger;
};

WidgetAnalyticsController::Private::ObjectInfo
    WidgetAnalyticsController::Private::objectInfoFromLayoutItem(const QnLayoutItemData& item)
{
    ObjectInfo objectInfo;

    objectInfo.id = qnResourceRuntimeDataManager->layoutItemData(
        item.uuid, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

    objectInfo.zoomWindowItemUuid = item.zoomTargetUuid;
    objectInfo.color = qnResourceRuntimeDataManager->layoutItemData(
        item.uuid, Qn::ItemFrameDistinctionColorRole).value<QColor>();

    objectInfo.rectangle = qnResourceRuntimeDataManager->layoutItemData(
        item.uuid, Qn::ItemAnalyticsModeSourceRegionRole).value<QRectF>();
    if (objectInfo.rectangle.isEmpty())
        objectInfo.rectangle = item.zoomRect;

    return objectInfo;
}

QRectF WidgetAnalyticsController::Private::zoomWindowRectangle(const QRectF& objectRect)
{
    return Geometry::movedInto(
        Geometry::expanded(1.0, objectRect, Qt::KeepAspectRatioByExpanding),
        QRectF(0, 0, 1, 1));
}

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

void WidgetAnalyticsController::Private::findExistingItems()
{
    auto handleLayoutItem =
        [this](const QnLayoutItemData& item)
        {
            const auto id = qnResourceRuntimeDataManager->layoutItemData(
                item.uuid, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();
            if (!id.isNull())
                objectInfoById[id] = objectInfoFromLayoutItem(item);
        };

    if (!zoomWindowObjectId.isNull())
    {
        handleLayoutItem(mediaResourceWidget->item()->data());
    }
    else
    {
        for (const auto& item: layoutResource()->getItems())
            handleLayoutItem(item);
    }
}

void WidgetAnalyticsController::Private::updateZoomWindowForArea(const QnUuid& areaId)
{
    auto item = findItemForObject(layoutResource(), areaId);
    const bool newItem = item.uuid.isNull();

    if (newItem && !objectInfoById.contains(areaId))
        return;

    // After switching layouts the controller is re-created and all cached information is cleared.
    // However we can restore object info from the existing layout item.
    auto& object = objectInfoById[areaId];
    if (object.id.isNull())
        object = objectInfoFromLayoutItem(item);

    if (newItem)
    {
        if (!object.zoomWindowItemUuid.isNull())
        {
            item.uuid = object.zoomWindowItemUuid;
        }
        else
        {
            item.uuid = QnUuid::createUuid();
            object.zoomWindowItemUuid = item.uuid;
        }
        item.flags = Qn::PendingGeometryAdjustment;
        item.resource.id = mediaResourceWidget->resource()->toResourcePtr()->getId();
        item.zoomTargetUuid = item.resource.id;
        const auto targetPoint = mediaResourceWidget->item()->combinedGeometry().bottomRight();
        item.combinedGeometry = QRectF(targetPoint, targetPoint);

        qnResourceRuntimeDataManager->setLayoutItemData(
            item.uuid, Qn::ItemAnalyticsModeRegionIdRole, areaId);
        qnResourceRuntimeDataManager->setLayoutItemData(
            item.uuid, Qn::ItemZoomWindowRectangleVisibleRole, false);
    }

    item.zoomRect = zoomWindowRectangle(object.rectangle);

    qnResourceRuntimeDataManager->setLayoutItemData(
        item.uuid, Qn::ItemFrameDistinctionColorRole, object.color);
    qnResourceRuntimeDataManager->setLayoutItemData(
        item.uuid, Qn::ItemAnalyticsModeSourceRegionRole, object.rectangle);

    if (newItem)
        layoutResource()->addItem(item);
    else
        layoutResource()->updateItem(item);
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

void WidgetAnalyticsController::Private::removeAreaAndZoomWindow(ObjectInfo& object)
{
    areaHighlightWidget->removeArea(object.id);

    if (!object.zoomWindowItemUuid.isNull())
    {
        layoutResource()->removeItem(object.zoomWindowItemUuid);
        object.zoomWindowItemUuid = QnUuid();
    }
}

void WidgetAnalyticsController::Private::updateObjectAreas(microseconds timestamp)
{
    if (!zoomWindowObjectId.isNull())
    {
        const auto& objectInfo = objectInfoById.value(zoomWindowObjectId);
        auto areaInfo = areaInfoFromObject(objectInfo);
        areaInfo.rectangle = Geometry::toSubRect(
            mediaResourceWidget->zoomRect(), objectInfo.rectangle);

        areaHighlightWidget->addOrUpdateArea(areaInfo);
        return;
    }

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

        if (!objectInfo.zoomWindowItemUuid.isNull())
        {
            const auto& layout = layoutResource();
            auto item = layout->getItem(objectInfo.zoomWindowItemUuid);
            if (!item.uuid.isNull())
            {
                item.zoomRect = zoomWindowRectangle(areaInfo.rectangle);
                qnResourceRuntimeDataManager->setLayoutItemData(
                    item.uuid, Qn::ItemAnalyticsModeSourceRegionRole, areaInfo.rectangle);
                layout->updateItem(item);
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

    d->zoomWindowObjectId = qnResourceRuntimeDataManager->layoutItemData(
        mediaResourceWidget->item()->uuid(), Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

    d->findExistingItems();


    if (nx::analytics::loggingIni().isLoggingEnabled())
    {
        d->logger = std::make_unique<nx::analytics::MetadataLogger>(
            "widget_analytics_controller_",
            kWidgetAnalyticsControllerFrameLogPattern,
            kWidgetAnalyticsControllerMetadataLogPattern,
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

    if (!d->zoomWindowObjectId.isNull())
        return;

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
            d->removeAreaAndZoomWindow(*it);
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
    if (!d->zoomWindowObjectId.isNull())
        return;

    for (auto& objectInfo: d->objectInfoById)
        d->removeAreaAndZoomWindow(objectInfo);
}

void WidgetAnalyticsController::updateAreaForZoomWindow()
{
    if (d->zoomWindowObjectId.isNull())
        return;

    auto& objectInfo = d->objectInfoById[d->zoomWindowObjectId];
    if (objectInfo.id.isNull())
        return;

    objectInfo.rectangle = qnResourceRuntimeDataManager->layoutItemData(
        d->mediaResourceWidget->item()->uuid(),
        Qn::ItemAnalyticsModeSourceRegionRole).value<QRectF>();

    auto areaInfo = d->areaInfoFromObject(objectInfo);
    areaInfo.rectangle = Geometry::toSubRect(
        d->mediaResourceWidget->zoomRect(), objectInfo.rectangle);

    d->areaHighlightWidget->addOrUpdateArea(areaInfo);
    d->areaHighlightWidget->setHighlightedArea(d->zoomWindowObjectId);
}

void WidgetAnalyticsController::setAnalyticsMetadataProvider(
    const core::AbstractAnalyticsMetadataProviderPtr& provider)
{
    d->metadataProvider = provider;
}

void WidgetAnalyticsController::setAreaHighlightOverlayWidget(AreaHighlightOverlayWidget* widget)
{
    if (d->areaHighlightWidget)
        d->areaHighlightWidget->disconnect(d.data());

    d->areaHighlightWidget = widget;

    if (widget && d->zoomWindowObjectId.isNull())
    {
        QObject::connect(widget, &AreaHighlightOverlayWidget::areaClicked, d.data(),
            &Private::updateZoomWindowForArea);
    }
}

} // namespace nx::vms::client::desktop
