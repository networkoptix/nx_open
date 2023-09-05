// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_synchronizer.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/delete_later.h>
#include <utils/common/scoped_value_rollback.h>

#include "workbench_item.h"
#include "workbench_layout.h"

using namespace nx::vms::client::desktop;

QnWorkbenchLayoutSynchronizer::QnWorkbenchLayoutSynchronizer(
    QnWorkbenchLayout* layout,
    QObject* parent)
    :
    base_type(parent),
    m_layout(layout)
{
    if (!NX_ASSERT(layout, "Invalid layout"))
        return;

    initialize();
}

QnWorkbenchLayoutSynchronizer::~QnWorkbenchLayoutSynchronizer()
{
    NX_ASSERT(m_layout != nullptr && !resource().isNull());

    NX_VERBOSE(this, "Removing synchronizer for %1", resource());

    submitPendingItems();

    m_update = m_submit = false;

    m_layout->disconnect(this);
    resource()->disconnect(this);
}

LayoutResourcePtr QnWorkbenchLayoutSynchronizer::resource() const
{
    return m_layout->resource();
}

void QnWorkbenchLayoutSynchronizer::initialize()
{
    auto resource = this->resource().get();
    NX_VERBOSE(this, "Creating synchronizer for %1", this->resource());

    connect(m_layout,
        &QnWorkbenchLayout::itemAdded,
        this,
        &QnWorkbenchLayoutSynchronizer::at_layout_itemAdded);
    connect(m_layout,
        &QnWorkbenchLayout::itemRemoved,
        this,
        &QnWorkbenchLayoutSynchronizer::at_layout_itemRemoved);

    // TODO: #sivanov Get rid of resourceChanged.
    connect(resource,
        &QnLayoutResource::resourceChanged,
        this,
        &QnWorkbenchLayoutSynchronizer::at_resource_resourceChanged);

    connect(resource,
        &QnLayoutResource::itemAdded,
        this,
        &QnWorkbenchLayoutSynchronizer::at_resource_itemAdded);
    connect(resource,
        &QnLayoutResource::itemRemoved,
        this,
        &QnWorkbenchLayoutSynchronizer::at_resource_itemRemoved);
    connect(resource,
        &QnLayoutResource::itemChanged,
        this,
        &QnWorkbenchLayoutSynchronizer::at_resource_itemChanged);

    m_update = m_submit = true;
}

QnWorkbenchLayoutSynchronizer* QnWorkbenchLayoutSynchronizer::instance(
    const LayoutResourcePtr& resource)
{
    if (auto layout = QnWorkbenchLayout::instance(resource))
        return layout->layoutSynchronizer();

    return nullptr;
}

void QnWorkbenchLayoutSynchronizer::update()
{
    if (!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    m_layout->update(resource());
}

void QnWorkbenchLayoutSynchronizer::reset()
{
    update();
    m_pendingItems.clear();
}

void QnWorkbenchLayoutSynchronizer::submit()
{
    if (!m_submit)
        return;

    m_submitPending = false;
    m_pendingItems.clear();
    QScopedValueRollback<bool> guard(m_update, false);
    m_layout->submit(resource());
}

void QnWorkbenchLayoutSynchronizer::submitPendingItems()
{
    m_submitPending = false;

    QScopedValueRollback<bool> guard(m_update, false);

    for (const QnUuid& uuid: m_pendingItems)
    {
        QnWorkbenchItem* item = m_layout->item(uuid);
        if (item == nullptr)
            continue;

        QnResourcePtr resource = item->resource();
        NX_ASSERT(resource);
        if (!resource)
            continue;

        this->resource()->updateItem(item->data());
    }

    m_pendingItems.clear();
}

void QnWorkbenchLayoutSynchronizer::submitPendingItemsLater()
{
    if (m_submitPending)
        return;

    m_submitPending = true;
    executeDelayedParented([this] { submitPendingItems(); }, this);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSynchronizer::at_resource_resourceChanged()
{
    NX_VERBOSE(this, "Layout resource changed, %1", resource());

    update(); // TODO: #sivanov Check why there is no update guard here.
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemAdded(
    const QnLayoutResourcePtr& layout, const nx::vms::common::LayoutItemData& itemData)
{
    if (!m_update)
        return;

    NX_VERBOSE(this, "Item added to layout %1, resource id %2", layout, itemData.resource.id);

    /*
     * Check if was called back from at_layout_itemAdded because of layout resource living in a
     * different thread.
     */
    if (m_layout->item(itemData.uuid))
        return;

    const auto resource = getResourceByDescriptor(itemData.resource);
    if (!resource)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    auto item = new QnWorkbenchItem(resource, itemData, this);
    m_layout->addItem(item);
    if (auto zoomTargetItem = m_layout->item(itemData.zoomTargetUuid))
        m_layout->addZoomLink(item, zoomTargetItem);
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemRemoved(
    const QnLayoutResourcePtr& layout, const nx::vms::common::LayoutItemData& itemData)
{
    if (!m_update)
        return;

    NX_VERBOSE(this, "Item removed from layout %1, resource id %2", layout, itemData.resource.id);

    QScopedValueRollback<bool> guard(m_submit, false);

    QnWorkbenchItem* item = m_layout->item(itemData.uuid);
    if (item == nullptr)
    {
        /* We can get here in the following situation:
         *
         * Item was removed from layout, then it was removed from resource.
         * Resource was in another thread, so we have received removal event
         * outside the update guard. In this case there is nothing to do and we
         * should just return. */
        return;
    }

    m_pendingItems.remove(item->uuid());

    m_layout->removeItem(item); /* It is important to remove the item here, not inside the
                                   deleteLater callback, as we're under submit guard. */
    qnDeleteLater(item);
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemChanged(
    const QnLayoutResourcePtr&, const nx::vms::common::LayoutItemData& itemData)
{
    if (!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);

    QnWorkbenchItem* item = m_layout->item(itemData.uuid);
    if (item == nullptr)
    {
        /* We can get here in the following situation:
         *
         * An item is added to the layout resource, and its geometry is adjusted.
         * Resource is synchronized with the layout, and a new layout item is
         * created. However, due to limit on the number of layout items, it
         * is deleted right away.
         *
         * Because of resource and layout residing in different threads,
         * item change signal may get received when the layout item is
         * already destroyed. So, simply leaving is perfectly fine here. */
        return;
    }

    item->update(itemData);
}

void QnWorkbenchLayoutSynchronizer::at_layout_itemAdded(QnWorkbenchItem* item)
{
    connect(item,
        &QnWorkbenchItem::dataChanged,
        this,
        &QnWorkbenchLayoutSynchronizer::at_item_dataChanged);
    connect(item,
        &QnWorkbenchItem::flagChanged,
        this,
        &QnWorkbenchLayoutSynchronizer::at_item_flagChanged);

    if (!m_submit)
        return;

    QnResourcePtr resource = item->resource();
    if (!NX_ASSERT(resource))
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    this->resource()->addItem(item->data());
}

void QnWorkbenchLayoutSynchronizer::at_layout_itemRemoved(QnWorkbenchItem* item)
{
    item->disconnect(this);

    if (!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);

    m_pendingItems.remove(item->uuid());

    resource()->removeItem(item->uuid());
}

void QnWorkbenchLayoutSynchronizer::at_item_dataChanged(int role)
{
    if (role == Qn::ItemFlagsRole)
        return; /* This one is handled separately. */

    if (!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    QnWorkbenchItem* item = checked_cast<QnWorkbenchItem*>(sender());
    m_pendingItems.insert(item->uuid());
    submitPendingItemsLater();
}

void QnWorkbenchLayoutSynchronizer::at_item_flagChanged(Qn::ItemFlag flag, bool value)
{
    if (!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    QnWorkbenchItem* item = checked_cast<QnWorkbenchItem*>(sender());
    if (item->hasFlag(flag) != value)
        return; /* Somebody has already changed it back. */

    m_pendingItems.insert(item->uuid());
    submitPendingItemsLater();
}
