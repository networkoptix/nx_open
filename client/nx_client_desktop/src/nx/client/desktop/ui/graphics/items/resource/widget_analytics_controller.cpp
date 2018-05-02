#include "widget_analytics_controller.h"

#include <chrono>

#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>

#include <common/common_module.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>

#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/client/desktop/ui/graphics/items/overlays/area_highlight_overlay_widget.h>
#include <nx/client/desktop/analytics/object_display_settings.h>

#include <utils/math/linear_combination.h>

#include <ini.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr std::chrono::milliseconds kObjectTimeToLive = std::chrono::minutes(1);
static constexpr std::chrono::microseconds kFutureMetadataLength = std::chrono::seconds(2);
static constexpr int kMaxFutureMetadataPackets = 4;

QString objectDescription(const std::vector<common::metadata::Attribute>& attributes)
{
    QString result;

    bool first = true;

    for (const auto& attribute: attributes)
    {
        if (first)
            first = false;
        else
            result += L'\n';

        result += attribute.name;
        result += L'\t';
        result += attribute.value;
    }

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
    const qint64 rectangleTimestamp,
    const QRectF& futureRectangle,
    const qint64 futureRectangleTimestamp,
    const qint64 timestamp)
{
    if (timestamp < 0
        || rectangleTimestamp < 0
        || futureRectangleTimestamp < 0
        || timestamp < rectangleTimestamp
        || timestamp > futureRectangleTimestamp)
    {
        return rectangle;
    }

    const auto factor =
        static_cast<qreal>(timestamp - rectangleTimestamp)
            / static_cast<qreal>(futureRectangleTimestamp - rectangleTimestamp);

    return linearCombine(1 - factor, rectangle, factor, futureRectangle);
}

} // namespace

class WidgetAnalyticsController::Private: public QObject, public QnCommonModuleAware
{
public:
    struct ObjectInfo
    {
        QColor color;
        bool active = true;
        qint64 lastUsedTime = -1;
        QnUuid zoomWindowItemUuid;
        QRectF rectangle;
        qint64 metadataTimestamp = -1;
        QString basicDescription;
        QString description;

        QRectF futureRectangle;
        qint64 futureRectangleTimestamp = -1;
    };

    Private(QnCommonModule* commonModule);

    void at_areaClicked(const QnUuid& areaId);

    void findExistingItems();

    QnLayoutResourcePtr layoutResource() const;

    QnUuid updateObjectInfoFromLayoutItem(const QnUuid& itemId);
    ObjectInfo& addOrUpdateObject(const nx::common::metadata::DetectedObject& object);
    void removeObject(const QnUuid& objectId);

    void updateObjectAreas(qint64 timestamp);

    QRectF zoomWindowRectangle(const QRectF& objectRect) const;

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    QnUuid layoutId;
    QnUuid zoomWindowObjectId;
    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AreaHighlightOverlayWidget> areaHighlightWidget;

    QHash<QnUuid, ObjectInfo> objectInfoById;
    QElapsedTimer timer;
};

WidgetAnalyticsController::Private::Private(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void WidgetAnalyticsController::Private::at_areaClicked(const QnUuid& areaId)
{
    auto item = findItemForObject(layoutResource(), areaId);

    auto it = objectInfoById.find(areaId);
    if (it == objectInfoById.end())
    {
        if (item.uuid.isNull())
            return;

        updateObjectInfoFromLayoutItem(item.uuid);
    }

    auto& object = *it;

    bool newItem = item.uuid.isNull();

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
        item.zoomTargetUuid = mediaResourceWidget->resource()->toResourcePtr()->getId();
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

void WidgetAnalyticsController::Private::findExistingItems()
{
    const auto currentTime = timer.elapsed();

    auto handleLayoutItem = [this, currentTime](const QnUuid& itemId)
        {
            const auto id = qnResourceRuntimeDataManager->layoutItemData(
                itemId, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

            if (id.isNull())
                return;

            updateObjectInfoFromLayoutItem(itemId);

            auto& objectInfo = objectInfoById[id];
            objectInfo.active = true;
            objectInfo.lastUsedTime = currentTime;
        };

    if (!zoomWindowObjectId.isNull())
    {
        handleLayoutItem(mediaResourceWidget->item()->uuid());
    }
    else
    {
        for (const auto& item: layoutResource()->getItems())
            handleLayoutItem(item.uuid);
    }
}

QnLayoutResourcePtr WidgetAnalyticsController::Private::layoutResource() const
{
    return mediaResourceWidget->item()->layout()->resource();
}

QnUuid WidgetAnalyticsController::Private::updateObjectInfoFromLayoutItem(const QnUuid& itemId)
{
    const auto item = layoutResource()->getItem(itemId);

    if (item.uuid.isNull())
        return QnUuid();

    const auto objectId = qnResourceRuntimeDataManager->layoutItemData(
        itemId, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

    if (objectId.isNull())
        return objectId;

    auto& objectInfo = objectInfoById[objectId];
    objectInfo.rectangle = qnResourceRuntimeDataManager->layoutItemData(
        itemId, Qn::ItemAnalyticsModeSourceRegionRole).value<QRectF>();
    if (objectInfo.rectangle.isEmpty())
        objectInfo.rectangle = item.zoomRect;

    objectInfo.zoomWindowItemUuid = item.zoomTargetUuid;
    objectInfo.color = qnResourceRuntimeDataManager->layoutItemData(
        item.uuid, Qn::ItemFrameDistinctionColorRole).value<QColor>();

    return objectId;
}

WidgetAnalyticsController::Private::ObjectInfo&
    WidgetAnalyticsController::Private::addOrUpdateObject(
        const common::metadata::DetectedObject& object)
{
    const auto settings = commonModule()->findInstance<ObjectDisplaySettings>();

    auto& objectInfo = objectInfoById[object.objectId];
    objectInfo.color = settings->objectColor(object);

    objectInfo.rectangle = object.boundingBox;
    objectInfo.basicDescription = objectDescription(settings->briefAttributes(object));
    objectInfo.description = objectDescription(settings->visibleAttributes(object));
    objectInfo.active = true;

    return objectInfo;
}

void WidgetAnalyticsController::Private::removeObject(const QnUuid& objectId)
{
    areaHighlightWidget->removeArea(objectId);

    const auto it = objectInfoById.find(objectId);
    if (it == objectInfoById.end())
        return;

    it->active = false;

    if (!it->zoomWindowItemUuid.isNull())
    {
        layoutResource()->removeItem(it->zoomWindowItemUuid);
        it->zoomWindowItemUuid = QnUuid();
    }
}

void WidgetAnalyticsController::Private::updateObjectAreas(qint64 timestamp)
{
    for (auto it = objectInfoById.begin(); it != objectInfoById.end(); ++it)
    {
        const auto& objectInfo = *it;

        if (!objectInfo.active)
            continue;

        AreaHighlightOverlayWidget::AreaInformation areaInfo;
        areaInfo.id = it.key();
        areaInfo.color = objectInfo.color;
        areaInfo.text = objectInfo.basicDescription;
        areaInfo.hoverText = objectInfo.description;

        if (ini().displayAnalyticsDelay && objectInfo.metadataTimestamp > 0)
        {
            areaInfo.hoverText +=
                lit("\nDelay\t%1").arg((timestamp - objectInfo.metadataTimestamp) / 1000);
        }

        if (!zoomWindowObjectId.isNull())
        {
            areaInfo.rectangle = core::Geometry::toSubRect(
                mediaResourceWidget->zoomRect(), objectInfo.rectangle);
        }
        else
        {
            if (ini().enableDetectedObjectsInterpolation)
            {
                areaInfo.rectangle = interpolatedRectangle(
                    objectInfo.rectangle,
                    objectInfo.metadataTimestamp,
                    objectInfo.futureRectangle,
                    objectInfo.futureRectangleTimestamp,
                    timestamp);
            }
            else
            {
                areaInfo.rectangle = objectInfo.rectangle;
            }

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
        }

        areaHighlightWidget->addOrUpdateArea(areaInfo);
    }
}

QRectF WidgetAnalyticsController::Private::zoomWindowRectangle(const QRectF& objectRect) const
{
    return core::Geometry::movedInto(
        core::Geometry::expanded(1.0, objectRect, Qt::KeepAspectRatioByExpanding),
        QRectF(0, 0, 1, 1));
}

WidgetAnalyticsController::WidgetAnalyticsController(QnMediaResourceWidget* mediaResourceWidget):
    base_type(mediaResourceWidget),
    d(new Private(commonModule()))
{
    NX_ASSERT(mediaResourceWidget);
    d->mediaResourceWidget = mediaResourceWidget;

    d->zoomWindowObjectId = qnResourceRuntimeDataManager->layoutItemData(
        mediaResourceWidget->item()->uuid(), Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

    d->timer.restart();

    d->findExistingItems();
}

WidgetAnalyticsController::~WidgetAnalyticsController()
{
}

void WidgetAnalyticsController::updateAreas(qint64 timestamp, int channel)
{
    if (!d->metadataProvider || !d->areaHighlightWidget || !d->zoomWindowObjectId.isNull())
        return;

    const auto elapsed = d->timer.elapsed();

    auto metadataList = d->metadataProvider->metadataRange(
        timestamp, timestamp + kFutureMetadataLength.count(), channel, kMaxFutureMetadataPackets);

    QSet<QnUuid> objectIds;

    if (!metadataList.isEmpty())
    {
        const auto metadata = metadataList.takeFirst();

        for (const auto& object: metadata->objects)
        {
            auto& objectInfo = d->addOrUpdateObject(object);
            objectInfo.metadataTimestamp = metadata->timestampUsec;
            objectInfo.lastUsedTime = elapsed;
            objectIds.insert(object.objectId);
        }
    }

    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); /*no increment*/)
    {
        if (!objectIds.contains(it.key()) && it->active)
            d->removeObject(it.key());

        if (it->active || elapsed - it->lastUsedTime < kObjectTimeToLive.count())
        {
            [&] // Find and update future rectangle for the object.
            {
                for (const auto& metadata: metadataList)
                {
                    for (const auto& object: metadata->objects)
                    {
                        if (object.objectId == it.key())
                        {
                            it->futureRectangle = object.boundingBox;
                            it->futureRectangleTimestamp = metadata->timestampUsec;
                            return;
                        }
                    }
                }
            }();

            ++it;
        }
        else
        {
            it = d->objectInfoById.erase(it);
        }
    }

    d->updateObjectAreas(timestamp);
}

void WidgetAnalyticsController::clearAreas()
{
    if (!d->zoomWindowObjectId.isNull())
        return;

    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); ++it)
        d->removeObject(it.key());
}

void WidgetAnalyticsController::updateAreaForZoomWindow()
{
    if (d->zoomWindowObjectId.isNull())
        return;

    auto it = d->objectInfoById.find(d->zoomWindowObjectId);
    if (it == d->objectInfoById.end())
        return;

    it->rectangle = qnResourceRuntimeDataManager->layoutItemData(
        d->mediaResourceWidget->item()->uuid(),
        Qn::ItemAnalyticsModeSourceRegionRole).value<QRectF>();
    d->updateObjectAreas(it->metadataTimestamp);
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
            &Private::at_areaClicked);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
