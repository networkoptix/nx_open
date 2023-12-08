// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "external.h"

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop::integrations::c2p::jsapi {

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

struct External::Private: public QObject, public WindowContextAware
{
    Private(External* q, QnWorkbenchItem* item);

    void resetC2pLayout(
        const QnVirtualCameraResourceList& cameras,
        std::chrono::milliseconds timestamp);

    External* const q;
    const QPointer<QnWorkbenchLayout> layout;
    const QPointer<QnWorkbenchItem> widgetItem;
};

External::Private::Private(External* q, QnWorkbenchItem* item):
    WindowContextAware(item->layout()->windowContext()),
    q(q),
    layout(item->layout()),
    widgetItem(item)
{
}

void External::Private::resetC2pLayout(
    const QnVirtualCameraResourceList& cameras,
    std::chrono::milliseconds timestamp)
{
    if (!layout || !widgetItem)
        return;

    const auto currentItemId = widgetItem->uuid();
    auto currentLayout = layout->resource();
    if (!NX_ASSERT(currentLayout))
        return;

    auto existingItems = currentLayout->getItems();

    common::LayoutItemDataList items;

    const int size = std::max((int) cameras.size(), 1);

    auto currentItem = widgetItem->data();
    currentItem.combinedGeometry = QRectF(0, 0, size, size);
    items.push_back(currentItem);

    int y = 0;
    for (const auto& camera: cameras)
    {
        common::LayoutItemData item = layoutItemFromResource(camera);

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

    // Select camera as an active item to display timeline.
    QnUuid activeItemId;
    const auto allItems = currentLayout->getItems();
    for (const auto& item: allItems)
    {
        if (item.uuid == currentItemId)
            continue;

        currentLayout->setItemData(item.uuid, Qn::ItemPausedRole, true);
        currentLayout->setItemData(item.uuid, Qn::ItemTimeRole,
            QVariant::fromValue<qint64>(timestamp.count()));
        currentLayout->setItemData(item.uuid, Qn::ItemSliderWindowRole,
            QVariant::fromValue(sliderWindow));
        currentLayout->setItemData(item.uuid, Qn::ItemSliderSelectionRole, {});

        activeItemId = item.uuid;
    }

    if (!activeItemId.isNull())
    {
        workbench()->setItem(
            Qn::CentralRole,
            workbench()->currentLayout()->item(activeItemId));
    }

    // If at least one camera is opened, navigate to the given time.
    if (!navigator()->isPlayingSupported())
        return;

    // Forcefully enable sync.
    std::chrono::microseconds timestampUs = timestamp;
    auto streamSynchronizer = windowContext()->streamSynchronizer();
    streamSynchronizer->start(/*timeUSec*/ timestampUs.count(), /*speed*/ 0.0);

    navigator()->setPlaying(false);
    navigator()->setLive(false);
    navigator()->setPosition(timestampUs.count());
}

//-------------------------------------------------------------------------------------------------

External::External(QnWorkbenchItem* item, QObject* parent):
    base_type(parent),
    d(new Private(this, item))
{
}

External::~External()
{
}

void External::c2pplayback(const QString& cameraNames, int timestampSec)
{
    QnVirtualCameraResourceList cameras;
    const auto pool = d->system()->resourcePool();

    const auto names = cameraNames.split(',', Qt::SkipEmptyParts);
    for (auto name: names)
    {
        name = name.toLower().trimmed();
        auto camera = camera_id_helper::findCameraByFlexibleId(pool, name)
            .dynamicCast<QnVirtualCameraResource>();

        if (!camera)
            camera = findCameraByName(pool, name);

        if (camera)
            cameras.push_back(camera);
    }

    const std::chrono::seconds timestamp(timestampSec);

    if (!cameras.empty())
        d->resetC2pLayout(cameras, timestamp);
}

} // namespace nx::vms::client::desktop::jsapi
