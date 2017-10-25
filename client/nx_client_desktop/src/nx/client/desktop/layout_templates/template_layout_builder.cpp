#include "template_layout_builder.h"

#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <client/client_globals.h>
#include <client_core/connection_context_aware.h>
#include <plugins/resource/avi/filetypesupport.h>
#include <plugins/resource/avi/avi_resource.h>
#include <core/resource/resource_directory_browser.h>
#include <ui/graphics/items/resource/resource_widget.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static int indexFromValue(const QString& value)
{
    bool ok;
    const int index = value.toInt(&ok);
    return ok ? index : -1;
}

} // namespace

class TemplateLayoutBuilder::Private: public QnConnectionContextAware
{
public:
    LayoutTemplate layoutTemplate;

    QHash<QString, QList<QnResourcePtr>> resourcesByType;

    int currentBlockX = 0;
    int currentBlockY = 0;

    QHash<QString, int> blockShiftForResourceType;
    mutable QHash<QString, int> newBlockShiftForResourceType;

    QList<ItemData> createdItems;
    int firstItemInBlock = 0;

public:
    void reset();
    bool haveUnprocessedResources() const;

    QnLayoutItemData createItemData(
        const LayoutTemplateItem& itemTemplate,
        const QnLayoutItemResourceDescriptor& resource,
        QnResourceWidget::Options options = static_cast<QnResourceWidget::Option>(0));
    QnLayoutItemData createItem(const LayoutTemplateItem& itemTemplate);
    QnLayoutItemData createResourceItem(const LayoutTemplateItem& itemTemplate);
    QnLayoutItemData createZoomWindowItem(const LayoutTemplateItem& itemTemplate);

    QnResourcePtr resourceForItem(const LayoutTemplateItemSource& source) const;
    QnResourcePtr resourceForImage(const QString& fileName) const;

    void createBlocks(const LayoutTemplate& layoutTemplate);
    void adjustItemsPosition(const LayoutTemplate& layoutTemplate);
    QnLayoutResourcePtr createLayout();
};

void TemplateLayoutBuilder::Private::reset()
{
    blockShiftForResourceType.clear();
    createdItems.clear();
    firstItemInBlock = 0;
}

bool TemplateLayoutBuilder::Private::haveUnprocessedResources() const
{
    for (auto it = blockShiftForResourceType.begin(); it != blockShiftForResourceType.end(); ++it)
    {
        if (it.value() < resourcesByType.value(it.key()).size())
            return true;
    }
    return false;
}

QnLayoutItemData TemplateLayoutBuilder::Private::createItemData(
    const LayoutTemplateItem& itemTemplate,
    const QnLayoutItemResourceDescriptor& resource,
    QnResourceWidget::Options options)
{
    QnLayoutItemData itemData;
    itemData.uuid = QnUuid::createUuid();
    itemData.flags = Qn::Pinned;
    itemData.combinedGeometry =
        QRect(itemTemplate.x, itemTemplate.y, itemTemplate.width, itemTemplate.height);
    itemData.resource = resource;

    if (!itemTemplate.visible)
        options |= QnResourceWidget::InvisibleWidgetOption;

    if (options)
    {
        qnResourceRuntimeDataManager->setLayoutItemData(
            itemData.uuid, Qn::ItemWidgetOptions, options);
    }

    if (itemTemplate.frameColor.isValid())
    {
        qnResourceRuntimeDataManager->setLayoutItemData(
            itemData.uuid, Qn::ItemFrameDistinctionColorRole, itemTemplate.frameColor);
    }

    return itemData;
}

QnLayoutItemData TemplateLayoutBuilder::Private::createItem(const LayoutTemplateItem& itemTemplate)
{
    if (itemTemplate.type == LayoutTemplateItem::kResourceItemType)
        return createResourceItem(itemTemplate);
    if (itemTemplate.type == LayoutTemplateItem::kZoomWindowItemType)
        return createZoomWindowItem(itemTemplate);

    return QnLayoutItemData();
}

QnLayoutItemData TemplateLayoutBuilder::Private::createResourceItem(
    const LayoutTemplateItem& itemTemplate)
{
    const auto& resource = resourceForItem(itemTemplate.source);
    if (!resource)
        return QnLayoutItemData();

    QnResourceWidget::Options options = static_cast<QnResourceWidget::Option>(0);
    if (layoutTemplate.type == LayoutTemplate::kAnalyticsLayoutType)
        options = QnResourceWidget::AnalyticsModeMaster;

    return createItemData(
        itemTemplate,
        QnLayoutItemResourceDescriptor{resource->getId(), resource->getUniqueId()},
        options);
}

QnLayoutItemData TemplateLayoutBuilder::Private::createZoomWindowItem(
    const LayoutTemplateItem& itemTemplate)
{
    if (itemTemplate.source.type != LayoutTemplateItem::kItemSourceType)
        return QnLayoutItemData();

    int index = indexFromValue(itemTemplate.source.value);
    if (itemTemplate.source.relativeToBlock)
        index += firstItemInBlock;

    if (createdItems.size() <= index)
        return QnLayoutItemData();

    const auto& sourceItem = createdItems[index];

    QnResourceWidget::Options options = static_cast<QnResourceWidget::Option>(0);
    if (layoutTemplate.type == LayoutTemplate::kAnalyticsLayoutType)
    {
        options = QnResourceWidget::WindowRotationForbidden
            | QnResourceWidget::InfoOverlaysForbidden
            | QnResourceWidget::AnalyticsModeSlave;
    }

    auto itemData = createItemData(
        itemTemplate,
        sourceItem.data.resource,
        options);

    itemData.zoomTargetUuid = sourceItem.data.uuid;

    return itemData;
}

QnResourcePtr TemplateLayoutBuilder::Private::resourceForItem(
    const LayoutTemplateItemSource& source) const
{
    if (source.type == LayoutTemplateItem::kFileSourceType)
    {
        const auto resource = QnResourceDirectoryBrowser::resourceFromFile(
            source.value, resourcePool());

        if (resource)
            resourcePool()->addResource(resource);

        return resource;
    }

    int index = indexFromValue(source.value);
    if (index < 0)
        return QnResourcePtr();

    if (source.relativeToBlock)
        index += blockShiftForResourceType.value(source.type);

    const auto& resourcesList = resourcesByType.value(source.type);
    if (resourcesList.size() <= index)
        return QnResourcePtr();

    auto& newBlockShift = newBlockShiftForResourceType[source.type];
    newBlockShift = std::max(newBlockShift, index + 1);

    return resourcesList[index];
}

QnResourcePtr TemplateLayoutBuilder::Private::resourceForImage(const QString& fileName) const
{
    if (!FileTypeSupport::isImageFileExt(fileName))
        return QnResourcePtr();

    QnResourcePtr resource(new QnAviResource(fileName));
    resource->addFlags(Qn::still_image);
    resource->removeFlags(Qn::video | Qn::audio);
    return resource;
}

void TemplateLayoutBuilder::Private::createBlocks(const LayoutTemplate& layoutTemplate)
{
    int currentBlock = 0;

    newBlockShiftForResourceType = blockShiftForResourceType;

    do
    {
        for (const LayoutTemplateItem& itemTemplate: layoutTemplate.block.items)
        {
            ItemData item;
            item.data = createItem(itemTemplate);

            if (item.data.uuid.isNull())
                continue;

            item.itemTemplate = itemTemplate;
            item.block = currentBlock;
            createdItems.append(item);
        }

        if (blockShiftForResourceType == newBlockShiftForResourceType)
        {
            while (createdItems.size() > firstItemInBlock)
                createdItems.removeLast();

            break;
        }

        blockShiftForResourceType = newBlockShiftForResourceType;
        firstItemInBlock = createdItems.size();
        ++currentBlock;
    } while (haveUnprocessedResources());
}

void TemplateLayoutBuilder::Private::adjustItemsPosition(const LayoutTemplate& layoutTemplate)
{
    if (createdItems.isEmpty())
        return;

    const int blockWidth = layoutTemplate.block.width;
    const int blockHeight = layoutTemplate.block.height;

    const int blockCount = createdItems.last().block + 1;

    // TODO: #dklychkov Use Grid walker and allow different block placement schemes.

    const int columns = static_cast<int>(std::ceil(std::sqrt(blockCount)));

    for (auto& item: createdItems)
    {
        const int itemX = static_cast<int>(item.data.combinedGeometry.x());
        const int itemY = static_cast<int>(item.data.combinedGeometry.y());

        const int blockX = item.block % columns;
        const int blockY = item.block / columns;

        item.data.combinedGeometry.setX(blockX * blockWidth + itemX);
        item.data.combinedGeometry.setY(blockY * blockHeight + itemY);
    }
}

QnLayoutResourcePtr TemplateLayoutBuilder::Private::createLayout()
{
    const QnLayoutResourcePtr layout(new QnLayoutResource());

    for (const auto& item: createdItems)
        layout->addItem(item.data);

    return layout;
}

TemplateLayoutBuilder::TemplateLayoutBuilder():
    d(new Private())
{
}

TemplateLayoutBuilder::~TemplateLayoutBuilder()
{
}

QnLayoutResourcePtr TemplateLayoutBuilder::buildLayout(const LayoutTemplate& layoutTemplate)
{
    d->reset();
    d->layoutTemplate = layoutTemplate;

    d->createBlocks(layoutTemplate);
    d->adjustItemsPosition(layoutTemplate);

    return d->createLayout();
}

void TemplateLayoutBuilder::setResources(
    const QString& type, const QList<QnResourcePtr>& resources)
{
    d->resourcesByType[type] = resources;
}

QList<TemplateLayoutBuilder::ItemData> TemplateLayoutBuilder::createdItems() const
{
    return d->createdItems;
}

} // namespace desktop
} // namespace client
} // namespace nx
