#include "c2p_resource_widget.h"

#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebPage>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/standard/graphics_web_view.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_runtime_data.h>

namespace nx {
namespace client {
namespace desktop {

C2pResourceWidget::C2pResourceWidget(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(context, item, parent)
{
    QWebFrame* frame = m_webView->page()->mainFrame();
    connect(frame, &QWebFrame::javaScriptWindowObjectCleared, this,
        [this, frame]()
        {
            frame->addToJavaScriptWindowObject(lit("external"), this);
        });
}

void C2pResourceWidget::c2pplayback(const QString& cameraNames, int timestamp)
{
    QnVirtualCameraResourceList cameras;
    for (auto name: cameraNames.split(L',', QString::SplitBehavior::SkipEmptyParts))
    {
        name = name.toLower().trimmed();
        if (const auto camera = resourcePool()->getResource<QnVirtualCameraResource>(
            [name](const QnVirtualCameraResourcePtr& resource)
            {
                return resource->getName().toLower() == name;
            }))
        {
            cameras.push_back(camera);
        }
    }
    if (!cameras.empty())
        resetC2pLayout(cameras, timestamp);
}

void C2pResourceWidget::resetC2pLayout(const QnVirtualCameraResourceList& cameras,
     qint64 timestampMs)
{
    const auto currentItemId = this->item()->uuid();
    auto currentLayout = item()->layout()->resource();
    auto existingItems = currentLayout->getItems();

    QnLayoutItemDataList items;

    const int size = std::max(cameras.size(), 1);

    auto currentItem = this->item()->data();
    currentItem.combinedGeometry = QRectF(0, 0, size, size);
    items.push_back(currentItem);

    int y = 0;
    for (const auto& camera: cameras)
    {
        QnLayoutItemData item;
        item.resource.id = camera->getId();
        item.resource.uniqueId = camera->getUniqueId();

        for (const auto& existingItem: existingItems)
        {
            if (existingItem.resource.id == item.resource.id)
            {
                item = existingItem;
                break;
            }
        }

        item.flags = Qn::Pinned;
        item.combinedGeometry = QRectF(size, y, 1, 1);
        y++;

        items.push_back(item);
    }

    // Uuids are generated here if needed.
    currentLayout->setItems(items);
    currentLayout->setCellSpacing(0);

    for (const auto& item: currentLayout->getItems())
    {
        if (item.uuid == currentItemId)
            continue;
        qnResourceRuntimeDataManager->setLayoutItemData(item.uuid, Qn::ItemTimeRole, timestampMs);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
