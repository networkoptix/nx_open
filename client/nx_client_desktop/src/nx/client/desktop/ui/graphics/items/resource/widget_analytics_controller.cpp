#include "widget_analytics_controller.h"

#include <chrono>

#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>

#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/utils/random.h>
#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/client/desktop/ui/graphics/items/overlays/area_highlight_overlay_widget.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr std::chrono::milliseconds kObjectTimeToLive = std::chrono::minutes(1);

static const QVector<QColor> kFrameColors{
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::darkRed,
    Qt::darkGreen,
    Qt::darkBlue,
    Qt::darkCyan,
    Qt::darkMagenta,
    Qt::darkYellow
};

QString objectDescription(const common::metadata::DetectedObject& object)
{
    QString result;

    bool first = true;

    for (const auto& attribute: object.labels)
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

QnUuid findItemForObject(const QnLayoutResourcePtr& layout, const QnUuid& objectId)
{
    if (objectId.isNull())
        return QnUuid();

    for (const auto& item: layout->getItems())
    {
        const auto id = qnResourceRuntimeDataManager->layoutItemData(
            item.uuid, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

        if (id == objectId)
            return item.uuid;
    }

    return QnUuid();
}

} // namespace

class WidgetAnalyticsController::Private: public QObject
{
public:
    struct ObjectInfo
    {
        QColor color;
        bool active = true;
        qint64 lastUsedTime = -1;
        QnUuid zoomWindowItemUuid;
        QRectF rectangle;
    };

    void at_areaClicked(const QnUuid& areaId);

    void findExistingItems();

    QnLayoutResourcePtr layoutResource() const;

    void updateObjectInfoFromLayoutItem(const QnUuid& itemId);
    ObjectInfo& addOrUpdateObject(const nx::common::metadata::DetectedObject& object);
    void removeObject(const QnUuid& objectId);

public:
    QnMediaResourceWidget* mediaResourceWidget = nullptr;
    QnUuid layoutId;
    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AreaHighlightOverlayWidget> areaHighlightWidget;

    QHash<QnUuid, ObjectInfo> objectInfoById;
    QElapsedTimer timer;
};

void WidgetAnalyticsController::Private::at_areaClicked(const QnUuid& areaId)
{
    auto it = objectInfoById.find(areaId);
    if (it == objectInfoById.end())
    {
        const auto& itemId = findItemForObject(layoutResource(), areaId);
        if (!itemId.isNull())
            return;

        updateObjectInfoFromLayoutItem(itemId);
    }

    auto& object = *it;

    if (!object.zoomWindowItemUuid.isNull())
        return;

    const auto layout = layoutResource();

    QnLayoutItemData item;
    item.uuid = QnUuid::createUuid();
    object.zoomWindowItemUuid = item.uuid;

    item.flags = Qn::PendingGeometryAdjustment;
    const auto targetPoint = mediaResourceWidget->item()->combinedGeometry().bottomRight();
    item.combinedGeometry = QRectF(targetPoint, targetPoint);
    item.resource.id = mediaResourceWidget->resource()->toResourcePtr()->getId();
    item.zoomRect = object.rectangle;
    item.zoomTargetUuid = mediaResourceWidget->resource()->toResourcePtr()->getId();

    qnResourceRuntimeDataManager->setLayoutItemData(
        item.uuid, Qn::ItemFrameDistinctionColorRole, object.color);
    qnResourceRuntimeDataManager->setLayoutItemData(
        item.uuid, Qn::ItemZoomWindowRectangleVisibleRole, false);
    qnResourceRuntimeDataManager->setLayoutItemData(
        item.uuid, Qn::ItemAnalyticsModeRegionIdRole, areaId);

    layout->addItem(item);
}

void WidgetAnalyticsController::Private::findExistingItems()
{
    const auto currentTime = timer.elapsed();

    for (const auto& item: layoutResource()->getItems())
    {
        const auto id = qnResourceRuntimeDataManager->layoutItemData(
            item.uuid, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

        if (id.isNull())
            continue;

        updateObjectInfoFromLayoutItem(item.uuid);

        auto& objectInfo = objectInfoById[id];
        objectInfo.active = true;
        objectInfo.lastUsedTime = currentTime;
    }
}

QnLayoutResourcePtr WidgetAnalyticsController::Private::layoutResource() const
{
    return mediaResourceWidget->item()->layout()->resource();
}

void WidgetAnalyticsController::Private::updateObjectInfoFromLayoutItem(const QnUuid& itemId)
{
    const auto item = layoutResource()->getItem(itemId);

    if (item.uuid.isNull())
        return;

    const auto objectId = qnResourceRuntimeDataManager->layoutItemData(
        itemId, Qn::ItemAnalyticsModeRegionIdRole).value<QnUuid>();

    if (objectId.isNull())
        return;

    auto& objectInfo = objectInfoById[objectId];
    objectInfo.rectangle = item.zoomRect;
    objectInfo.zoomWindowItemUuid = item.zoomTargetUuid;
    objectInfo.color = qnResourceRuntimeDataManager->layoutItemData(
        item.uuid, Qn::ItemFrameDistinctionColorRole).value<QColor>();
}

WidgetAnalyticsController::Private::ObjectInfo&
    WidgetAnalyticsController::Private::addOrUpdateObject(
        const common::metadata::DetectedObject& object)
{
    auto& objectInfo = objectInfoById[object.objectId];
    if (objectInfo.lastUsedTime < 0)
        objectInfo.color = utils::random::choice(kFrameColors);

    objectInfo.rectangle = object.boundingBox;
    objectInfo.active = true;

    AreaHighlightOverlayWidget::AreaInformation areaInfo;
    areaInfo.id = object.objectId;
    areaInfo.rectangle = object.boundingBox;
    areaInfo.color = objectInfo.color;
    areaInfo.text = objectDescription(object);

    areaHighlightWidget->addOrUpdateArea(areaInfo);

    if (!objectInfo.zoomWindowItemUuid.isNull())
    {
        const auto& layout = layoutResource();
        auto item = layout->getItem(objectInfo.zoomWindowItemUuid);
        item.zoomRect = areaInfo.rectangle;
        layout->updateItem(item);
    }

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

WidgetAnalyticsController::WidgetAnalyticsController(QnMediaResourceWidget* mediaResourceWidget):
    base_type(mediaResourceWidget),
    d(new Private())
{
    NX_ASSERT(mediaResourceWidget);
    d->mediaResourceWidget = mediaResourceWidget;

    d->timer.restart();

    d->findExistingItems();
}

WidgetAnalyticsController::~WidgetAnalyticsController()
{
}

void WidgetAnalyticsController::updateAreas(qint64 timestamp, int channel)
{
    if (!d->metadataProvider || !d->areaHighlightWidget)
        return;

    const auto elapsed = d->timer.elapsed();

    const auto metadata = d->metadataProvider->metadata(timestamp, channel);

    QSet<QnUuid> objectIds;

    if (metadata)
    {
        for (const auto& object: metadata->objects)
        {
            auto& objectInfo = d->addOrUpdateObject(object);
            objectInfo.lastUsedTime = elapsed;
            objectIds.insert(object.objectId);
        }
    }

    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); /*no increment*/)
    {
        if (!objectIds.contains(it.key()) && it->active)
            d->removeObject(it.key());

        if (it->active || elapsed - it->lastUsedTime < kObjectTimeToLive.count())
            ++it;
        else
            it = d->objectInfoById.erase(it);
    }
}

void WidgetAnalyticsController::clearAreas()
{
    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); ++it)
        d->removeObject(it.key());
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

    if (widget)
    {
        QObject::connect(widget, &AreaHighlightOverlayWidget::areaClicked, d.data(),
            &Private::at_areaClicked);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
