#include "workbench_layout_synchronizer.h"
#include <cassert>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delete_later.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include "workbench.h"
#include "workbench_layout.h"
#include "workbench_item.h"

namespace {
    const char *layoutSynchronizerPropertyName = "_qn_layoutSynchronizer";
}

Q_DECLARE_METATYPE(QnWorkbenchLayoutSynchronizer *);

typedef QHash<QnLayoutResource *, QnWorkbenchLayoutSynchronizer *> LayoutResourceSynchronizerHash;
Q_GLOBAL_STATIC(LayoutResourceSynchronizerHash, qn_synchronizerByLayoutResource)

QnWorkbenchLayoutSynchronizer::QnWorkbenchLayoutSynchronizer(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource, QObject *parent):
    QObject(parent),
    m_running(false),
    m_layout(layout),
    m_resource(resource),
    m_update(false),
    m_submit(false),
    m_autoDeleting(false),
    m_submitting(false)
{
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    } else if(!layout->property(layoutSynchronizerPropertyName).isNull()) {
        qnWarning("Given layout '%1' already has an associated layout synchronizer.", layout->name());
        return;
    }

    if(resource.isNull()) {
        qnNullWarning(resource);
        return;
    } else if(!resource->property(layoutSynchronizerPropertyName).isNull()) { 
        qnWarning("Given resource '%1' already has an associated layout synchronizer.", resource->getName());
        return;
    }

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
    if(m_layout == NULL)
        return;

    if(!m_resource.isNull())
        deinitialize();

    m_layout = NULL;

    m_pendingItems.clear();
}

void QnWorkbenchLayoutSynchronizer::clearResource() {
    if(m_resource.isNull())
        return;

    if(m_layout != NULL)
        deinitialize();

    m_resource.clear();
}

void QnWorkbenchLayoutSynchronizer::initialize() {
    assert(m_layout != NULL && !m_resource.isNull());

    qn_synchronizerByLayoutResource()->insert(m_resource.data(), this);
    m_layout->setProperty(layoutSynchronizerPropertyName, QVariant::fromValue<QnWorkbenchLayoutSynchronizer *>(this));

    connect(m_layout,           SIGNAL(itemAdded(QnWorkbenchItem *)),           this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
    connect(m_layout,           SIGNAL(itemRemoved(QnWorkbenchItem *)),         this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
    connect(m_layout,           SIGNAL(nameChanged()),                          this, SLOT(at_layout_nameChanged()));
    connect(m_layout,           SIGNAL(aboutToBeDestroyed()),                   this, SLOT(at_layout_aboutToBeDestroyed()));
    connect(m_resource.data(),  SIGNAL(resourceChanged()),                      this, SLOT(at_resource_resourceChanged()));
    connect(m_resource.data(),  SIGNAL(nameChanged()),                          this, SLOT(at_resource_nameChanged()));
    connect(m_resource.data(),  SIGNAL(itemAdded(const QnLayoutItemData &)),    this, SLOT(at_resource_itemAdded(const QnLayoutItemData &)));
    connect(m_resource.data(),  SIGNAL(itemRemoved(const QnLayoutItemData &)),  this, SLOT(at_resource_itemRemoved(const QnLayoutItemData &)));
    connect(m_resource.data(),  SIGNAL(itemChanged(const QnLayoutItemData &)),  this, SLOT(at_resource_itemChanged(const QnLayoutItemData &)));

    m_update = m_submit = true;
}

void QnWorkbenchLayoutSynchronizer::deinitialize() {
    assert(m_layout != NULL && !m_resource.isNull());

    submitPendingItems();

    m_update = m_submit = false;

    disconnect(m_layout, NULL, this, NULL);
    disconnect(m_resource.data(), NULL, this, NULL);

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

    QnScopedValueRollback<bool> guard(&m_submit, false);
    m_layout->load(m_resource);
}

void QnWorkbenchLayoutSynchronizer::submit() {
    if(!m_submit)
        return;

    m_submitting = false;
    m_pendingItems.clear();
    QnScopedValueRollback<bool> guard(&m_update, false);
    m_layout->save(m_resource);
}

void QnWorkbenchLayoutSynchronizer::submitPendingItems() {
    m_submitting = false;

    QnScopedValueRollback<bool> guard(&m_update, false);

    foreach(const QUuid &uuid, m_pendingItems) {
        QnWorkbenchItem *item = m_layout->item(uuid);
        if(item == NULL)
            continue;

        QnResourcePtr resource = qnResPool->getResourceByUniqId(item->resourceUid());
        if(!resource) {
            qnWarning("Item '%1' has no corresponding resource in the pool.", item->resourceUid());
            continue;
        }

        QnLayoutItemData itemData;
        itemData.resource.id = resource->getId();
        itemData.resource.path = resource->getUrl();
        itemData.uuid = item->uuid();
        item->save(itemData);

        m_resource->updateItem(itemData.uuid, itemData);
    }

    m_pendingItems.clear();
}

void QnWorkbenchLayoutSynchronizer::submitPendingItemsLater() {
    if(m_submitting)
        return;

    m_submitting = true;
    QMetaObject::invokeMethod(this, "submitPendingItems", Qt::QueuedConnection);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSynchronizer::at_resource_resourceChanged() {
    update();
}

void QnWorkbenchLayoutSynchronizer::at_resource_nameChanged() {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);
    m_layout->setName(m_resource->getName());
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemAdded(const QnLayoutItemData &itemData) {
    if(!m_update)
        return;

    if(m_layout->item(itemData.uuid) != NULL)
        return; /* Was called back from at_layout_itemAdded because of layout resource living in a different thread. */

    QnId id = itemData.resource.id;
    QString path = itemData.resource.path;

    QnResourcePtr resource;
    if (id.isValid())
        resource = qnResPool->getResourceById(id);
    else
        resource = qnResPool->getResourceByUniqId(path);

    if(resource.isNull()) {
        qnWarning("No resource in resource pool for id '%1' or path '%2'.", id.toString(), path);
        return;
    }

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), itemData.uuid);
    m_layout->addItem(item);
    item->load(itemData);
}

void QnWorkbenchLayoutSynchronizer::at_resource_itemRemoved(const QnLayoutItemData &itemData) {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QnWorkbenchItem *item = m_layout->item(itemData.uuid);
    if(item == NULL) {
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

void QnWorkbenchLayoutSynchronizer::at_resource_itemChanged(const QnLayoutItemData &itemData) {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);
    m_layout->item(itemData.uuid)->load(itemData);
}

void QnWorkbenchLayoutSynchronizer::at_layout_itemAdded(QnWorkbenchItem *item) {
    connect(item, SIGNAL(geometryChanged()),                            this, SLOT(at_item_changed()));
    connect(item, SIGNAL(geometryDeltaChanged()),                       this, SLOT(at_item_changed()));
    connect(item, SIGNAL(rotationChanged()),                            this, SLOT(at_item_changed()));
    connect(item, SIGNAL(flagChanged(QnWorkbenchItem::ItemFlag, bool)), this, SLOT(at_item_flagChanged(QnWorkbenchItem::ItemFlag, bool)));

    if(!m_submit)
        return;

    QnResourcePtr resource = qnResPool->getResourceByUniqId(item->resourceUid());
    if(!resource) {
        qnWarning("Item '%1' that was added to layout has no corresponding resource in the pool.", item->resourceUid());
        return;
    }

    QnScopedValueRollback<bool> guard(&m_update, false);

    QnLayoutItemData itemData;
    itemData.resource.id = resource->getId();
    itemData.resource.path = resource->getUrl();
    itemData.uuid = item->uuid();
    item->save(itemData);

    m_resource->addItem(itemData);
}

void QnWorkbenchLayoutSynchronizer::at_layout_itemRemoved(QnWorkbenchItem *item) {
    disconnect(item, NULL, this, NULL);

    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);

    m_pendingItems.remove(item->uuid());

    m_resource->removeItem(item->uuid());
}

void QnWorkbenchLayoutSynchronizer::at_layout_nameChanged() {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);
    m_resource->setName(m_layout->name());
}

void QnWorkbenchLayoutSynchronizer::at_layout_aboutToBeDestroyed() {
    clearLayout();
}

void QnWorkbenchLayoutSynchronizer::at_item_changed() {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);
    QnWorkbenchItem *item = checked_cast<QnWorkbenchItem *>(sender());
    m_pendingItems.insert(item->uuid());
    submitPendingItemsLater();
}

void QnWorkbenchLayoutSynchronizer::at_item_flagChanged(QnWorkbenchItem::ItemFlag flag, bool value) {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);
    QnWorkbenchItem *item = checked_cast<QnWorkbenchItem *>(sender());
    if(item->checkFlag(flag) != value)
        return; /* Somebody has already changed it back. */

    m_pendingItems.insert(item->uuid());
    submitPendingItemsLater();
}

