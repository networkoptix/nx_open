// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_synchronizer.h"

#include <cassert>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delete_later.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include "workbench.h"
#include "workbench_layout.h"
#include "workbench_item.h"

namespace {

const char* layoutSynchronizerPropertyName = "_qn_layoutSynchronizer";

} // namespace

using LayoutResourceSynchronizerHash = QHash<QnLayoutResource*, QnWorkbenchLayoutSynchronizer*>;
Q_GLOBAL_STATIC(LayoutResourceSynchronizerHash, qn_synchronizerByLayoutResource)

QnWorkbenchLayoutSynchronizer::QnWorkbenchLayoutSynchronizer(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource, QObject *parent):
    base_type(parent),
    m_layout(layout),
    m_resource(resource)
{
    NX_ASSERT(layout, "Invalid layout");
    if (!layout)
        return;

    NX_ASSERT(resource, "Invalid layout resource");
    if (!resource)
        return;

    NX_ASSERT(!instance(layout), "Layout already has a synchronizer");
    if (instance(layout))
        return;

    m_running = true;
    initialize();
}

QnWorkbenchLayoutSynchronizer::~QnWorkbenchLayoutSynchronizer() {
    clearLayout();
    clearResource();
}

void QnWorkbenchLayoutSynchronizer::setAutoDeleting(bool autoDeleting) {
    if(m_autoDeleting == autoDeleting)
        return;

    m_autoDeleting = autoDeleting;

    autoDeleteLater();
}

void QnWorkbenchLayoutSynchronizer::clearLayout() {
    if(m_layout == nullptr)
        return;

    if(!m_resource.isNull())
        deinitialize();

    m_layout = nullptr;

    m_pendingItems.clear();
}

void QnWorkbenchLayoutSynchronizer::clearResource() {
    if(m_resource.isNull())
        return;

    if(m_layout != nullptr)
        deinitialize();

    m_resource.clear();
}

void QnWorkbenchLayoutSynchronizer::initialize() {
    NX_ASSERT(m_layout != nullptr && !m_resource.isNull());

    qn_synchronizerByLayoutResource()->insert(m_resource.data(), this);
    m_layout->setProperty(layoutSynchronizerPropertyName, QVariant::fromValue<QnWorkbenchLayoutSynchronizer *>(this));

    connect(m_layout,   &QnWorkbenchLayout::itemAdded,              this, &QnWorkbenchLayoutSynchronizer::at_layout_itemAdded);
    connect(m_layout,   &QnWorkbenchLayout::itemRemoved,            this, &QnWorkbenchLayoutSynchronizer::at_layout_itemRemoved);
    connect(m_layout,   &QnWorkbenchLayout::nameChanged,            this, &QnWorkbenchLayoutSynchronizer::at_layout_nameChanged);
    connect(m_layout,   &QnWorkbenchLayout::cellAspectRatioChanged, this, &QnWorkbenchLayoutSynchronizer::at_layout_cellAspectRatioChanged);
    connect(m_layout,   &QnWorkbenchLayout::cellSpacingChanged,     this, &QnWorkbenchLayoutSynchronizer::at_layout_cellSpacingChanged);
    connect(m_layout,   &QnWorkbenchLayout::aboutToBeDestroyed,     this, &QnWorkbenchLayoutSynchronizer::at_layout_aboutToBeDestroyed);
    // TODO: #sivanov Get rid of resourceChanged.
    connect(m_resource, &QnLayoutResource::resourceChanged,         this, &QnWorkbenchLayoutSynchronizer::at_resource_resourceChanged);
    connect(m_resource, &QnLayoutResource::nameChanged,             this, &QnWorkbenchLayoutSynchronizer::at_resource_nameChanged);
    connect(m_resource, &QnLayoutResource::cellAspectRatioChanged,  this, &QnWorkbenchLayoutSynchronizer::at_resource_cellAspectRatioChanged);
    connect(m_resource, &QnLayoutResource::cellSpacingChanged,      this, &QnWorkbenchLayoutSynchronizer::at_resource_cellSpacingChanged);
    connect(m_resource, &QnLayoutResource::lockedChanged,           this, &QnWorkbenchLayoutSynchronizer::at_resource_lockedChanged);
    connect(m_resource, &QnLayoutResource::itemAdded,               this, &QnWorkbenchLayoutSynchronizer::at_resource_itemAdded);
    connect(m_resource, &QnLayoutResource::itemRemoved,             this, &QnWorkbenchLayoutSynchronizer::at_resource_itemRemoved);
    connect(m_resource, &QnLayoutResource::itemChanged,             this, &QnWorkbenchLayoutSynchronizer::at_resource_itemChanged);

    m_update = m_submit = true;
}

void QnWorkbenchLayoutSynchronizer::deinitialize() {
    NX_ASSERT(m_layout != nullptr && !m_resource.isNull());

    submitPendingItems();

    m_update = m_submit = false;

    m_layout->disconnect(this);
    m_resource->disconnect(this);

    qn_synchronizerByLayoutResource()->remove(m_resource.data());
    m_layout->setProperty(layoutSynchronizerPropertyName, QVariant());

    m_running = false;

    autoDeleteLater();
}

QnWorkbenchLayoutSynchronizer *QnWorkbenchLayoutSynchronizer::instance(QnWorkbenchLayout *layout) {
    return layout->property(layoutSynchronizerPropertyName).value<QnWorkbenchLayoutSynchronizer *>();
}

QnWorkbenchLayoutSynchronizer *QnWorkbenchLayoutSynchronizer::instance(const QnLayoutResourcePtr &resource) {
    return qn_synchronizerByLayoutResource()->value(resource.data());
}

void QnWorkbenchLayoutSynchronizer::autoDeleteLater() {
    if(m_autoDeleting)
        QMetaObject::invokeMethod(this, "autoDelete", Qt::QueuedConnection);
}

void QnWorkbenchLayoutSynchronizer::autoDelete() {
    if(m_autoDeleting && !m_running)
        qnDeleteLater(this);
}

void QnWorkbenchLayoutSynchronizer::update() {
    if(!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    m_layout->update(m_resource);
}

void QnWorkbenchLayoutSynchronizer::reset() {
    update();
    m_pendingItems.clear();
}

void QnWorkbenchLayoutSynchronizer::submit() {
    if(!m_submit)
        return;

    m_submitPending = false;
    m_pendingItems.clear();
    QScopedValueRollback<bool> guard(m_update, false);
    m_layout->submit(m_resource);
}

void QnWorkbenchLayoutSynchronizer::submitPendingItems() {
    m_submitPending = false;

    QScopedValueRollback<bool> guard(m_update, false);

    foreach(const QnUuid &uuid, m_pendingItems) {
        QnWorkbenchItem *item = m_layout->item(uuid);
        if(item == nullptr)
            continue;

        QnResourcePtr resource = item->resource();
        NX_ASSERT(resource);
        if(!resource)
            continue;

        m_resource->updateItem(item->data());
    }

    m_pendingItems.clear();
}

void QnWorkbenchLayoutSynchronizer::submitPendingItemsLater() {
    if(m_submitPending)
        return;

    m_submitPending = true;
    QMetaObject::invokeMethod(this, "submitPendingItems", Qt::QueuedConnection);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSynchronizer::at_resource_resourceChanged() {
    update(); // TODO: #sivanov Check why there is no update guard here.
}

void QnWorkbenchLayoutSynchronizer::at_resource_nameChanged() {
    if(!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    m_layout->setName(m_resource->getName());
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemAdded(const QnLayoutResourcePtr& /*layout*/,
    const QnLayoutItemData& itemData)
{
    if (!m_update)
        return;

    /*
     * Check if was called back from at_layout_itemAdded because of layout resource living in a
     * different thread.
     */
    if (m_layout->item(itemData.uuid))
        return;

    const auto resource = resourcePool()->getResourceByDescriptor(itemData.resource);
    if (!resource)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    auto item = new QnWorkbenchItem(resource, itemData, this);
    m_layout->addItem(item);
    if (auto zoomTargetItem = m_layout->item(itemData.zoomTargetUuid))
        m_layout->addZoomLink(item, zoomTargetItem);
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &itemData) {
    if(!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);

    QnWorkbenchItem *item = m_layout->item(itemData.uuid);
    if(item == nullptr) {
        /* We can get here in the following situation:
         *
         * Item was removed from layout, then it was removed from resource.
         * Resource was in another thread, so we have received removal event
         * outside the update guard. In this case there is nothing to do and we
         * should just return. */
        return;
    }

    m_pendingItems.remove(item->uuid());

    m_layout->removeItem(item); /* It is important to remove the item here, not inside the deleteLater callback, as we're under submit guard. */
    qnDeleteLater(item);
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemChanged(const QnLayoutResourcePtr &, const QnLayoutItemData &itemData) {
    if(!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);

    QnWorkbenchItem *item = m_layout->item(itemData.uuid);
    if(item == nullptr) {
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

void QnWorkbenchLayoutSynchronizer::at_resource_cellAspectRatioChanged() {
    if(!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    m_layout->setCellAspectRatio(m_resource->cellAspectRatio());
}

void QnWorkbenchLayoutSynchronizer::at_resource_cellSpacingChanged() {
    if(!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    m_layout->setCellSpacing(m_resource->cellSpacing());
}

void QnWorkbenchLayoutSynchronizer::at_resource_lockedChanged() {
    if(!m_update)
        return;

    QScopedValueRollback<bool> guard(m_submit, false);
    m_layout->setLocked(m_resource->locked());
}

void QnWorkbenchLayoutSynchronizer::at_layout_itemAdded(QnWorkbenchItem *item) {
    connect(item, &QnWorkbenchItem::dataChanged,    this, &QnWorkbenchLayoutSynchronizer::at_item_dataChanged);
    connect(item, &QnWorkbenchItem::flagChanged,    this, &QnWorkbenchLayoutSynchronizer::at_item_flagChanged);

    if(!m_submit)
        return;

    QnResourcePtr resource = item->resource();
    NX_ASSERT(resource);
    if (!resource)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    m_resource->addItem(item->data());
}

void QnWorkbenchLayoutSynchronizer::at_layout_itemRemoved(QnWorkbenchItem *item) {
    item->disconnect(this);

    if(!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);

    m_pendingItems.remove(item->uuid());

    m_resource->removeItem(item->uuid());
}

void QnWorkbenchLayoutSynchronizer::at_layout_nameChanged() {
    if(!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    m_resource->setName(m_layout->name());
}

void QnWorkbenchLayoutSynchronizer::at_layout_cellAspectRatioChanged() {
    if(!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    m_resource->setCellAspectRatio(m_layout->cellAspectRatio());
}

void QnWorkbenchLayoutSynchronizer::at_layout_cellSpacingChanged() {
    if(!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    m_resource->setCellSpacing(m_layout->cellSpacing());
}

void QnWorkbenchLayoutSynchronizer::at_layout_aboutToBeDestroyed() {
    m_resource->setData(m_layout->data());
    submitPendingItems();

    clearLayout();
}

void QnWorkbenchLayoutSynchronizer::at_item_dataChanged(int role) {
    if(role == Qn::ItemFlagsRole)
        return; /* This one is handled separately. */

    if(!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    QnWorkbenchItem *item = checked_cast<QnWorkbenchItem *>(sender());
    m_pendingItems.insert(item->uuid());
    submitPendingItemsLater();
}

void QnWorkbenchLayoutSynchronizer::at_item_flagChanged(Qn::ItemFlag flag, bool value) {
    if(!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);
    QnWorkbenchItem *item = checked_cast<QnWorkbenchItem *>(sender());
    if(item->hasFlag(flag) != value)
        return; /* Somebody has already changed it back. */

    m_pendingItems.insert(item->uuid());
    submitPendingItemsLater();
}

