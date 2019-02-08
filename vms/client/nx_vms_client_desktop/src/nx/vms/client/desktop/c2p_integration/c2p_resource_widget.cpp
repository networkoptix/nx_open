#include "c2p_resource_widget.h"

#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebPage>

#include <api/helpers/camera_id_helper.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/graphics/items/standard/graphics_web_view.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_navigator.h>

#include <ui/style/webview_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>

namespace {

using namespace std::literals::chrono_literals;

static const std::chrono::minutes kSliderWindow = 10min;

QnVirtualCameraResourcePtr findCameraByName(QnResourcePool* resourcePool, const QString& name)
{
    return resourcePool->getResource<QnVirtualCameraResource>(
        [name](const QnVirtualCameraResourcePtr& resource)
        {
            return resource->getName().toLower() == name;
        });
}

} // namespace

namespace nx::vms::client::desktop {

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

    NxUi::setupWebViewStyle(m_webView, NxUi::WebViewStyle::c2p);

    m_webView->page()->setLinkDelegationPolicy(QWebPage::DontDelegateLinks);
}

void C2pResourceWidget::c2pplayback(const QString& cameraNames, int timestampSec)
{
    QnVirtualCameraResourceList cameras;
    for (auto name: cameraNames.split(L',', QString::SplitBehavior::SkipEmptyParts))
    {
        name = name.toLower().trimmed();
        auto camera = camera_id_helper::findCameraByFlexibleId(resourcePool(), name)
            .dynamicCast<QnVirtualCameraResource>();

        if (!camera)
            camera = findCameraByName(resourcePool(), name);

        if (camera)
            cameras.push_back(camera);
    }

    const std::chrono::seconds timestamp(timestampSec);

    if (!cameras.empty())
        resetC2pLayout(cameras, timestamp);
}

void C2pResourceWidget::resetC2pLayout(const QnVirtualCameraResourceList& cameras,
     std::chrono::milliseconds timestamp)
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

    const QnTimePeriod sliderWindow(timestamp - kSliderWindow/2, kSliderWindow);

    const auto dataManager = qnResourceRuntimeDataManager;

    // Select camera as an active item to display timeline.
    QnUuid activeItemId;
    for (const auto& item: currentLayout->getItems())
    {
        if (item.uuid == currentItemId)
            continue;

        dataManager->setLayoutItemData(item.uuid, Qn::ItemPausedRole, true);
        dataManager->setLayoutItemData(item.uuid, Qn::ItemTimeRole, timestamp.count());
        dataManager->setLayoutItemData(item.uuid, Qn::ItemSliderWindowRole,
            qVariantFromValue(sliderWindow));
        dataManager->setLayoutItemData(item.uuid, Qn::ItemSliderSelectionRole, {});

        activeItemId = item.uuid;
    }

    if (!activeItemId.isNull())
        workbench()->setItem(Qn::CentralRole, workbench()->currentLayout()->item(activeItemId));

    // If at least one camera is opened, navigate to the given time.
    if (!navigator()->isPlayingSupported())
        return;

    // Forcefully enable sync.
    std::chrono::microseconds timestampUs = timestamp;
    const auto streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
    streamSynchronizer->start(/*timeUSec*/ timestampUs.count(), /*speed*/ 0.0);

    navigator()->setPlaying(false);
    navigator()->setLive(false);
    navigator()->setPosition(timestampUs.count());
}

} // namespace nx::vms::client::desktop
