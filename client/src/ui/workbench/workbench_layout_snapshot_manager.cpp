#include "workbench_layout_snapshot_manager.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include "workbench_context.h"
#include "workbench_layout_snapshot_storage.h"
#include "workbench_layout_synchronizer.h"

// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutReplyProcessor
// -------------------------------------------------------------------------- //
void detail::QnWorkbenchLayoutReplyProcessor::at_finished(int status, const QByteArray &errorString, const QnResourceList &, int handle) {
    /* Note that we may get reply of size 0 if appserver is down.
     * This is why we use stored list of layouts. */
    if(m_manager) {
        if(status == 0) {
            m_manager.data()->at_layouts_saved(m_resources);
        } else {
            m_manager.data()->at_layouts_saveFailed(m_resources);
        }
    }

    emit finished(status, errorString, QnResourceList(m_resources), handle);

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutSnapshotManager
// -------------------------------------------------------------------------- //
QnWorkbenchLayoutSnapshotManager::QnWorkbenchLayoutSnapshotManager(QObject *parent):    
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_storage(new QnWorkbenchLayoutSnapshotStorage(this))
{
    /* Start listening to changes. */
    connect(resourcePool(),  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(resourcePool(),  SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));

    foreach(const QnResourcePtr &resource, context()->resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnWorkbenchLayoutSnapshotManager::~QnWorkbenchLayoutSnapshotManager() {
    disconnect(resourcePool(), NULL, this, NULL);

    foreach(const QnResourcePtr &resource, context()->resourcePool()->getResources())
        at_resourcePool_resourceRemoved(resource);

    m_storage->clear();
    m_flagsByLayout.clear();
}

QnAppServerConnectionPtr QnWorkbenchLayoutSnapshotManager::connection() const {
    return QnAppServerConnectionFactory::createConnection();
}

Qn::LayoutFlags QnWorkbenchLayoutSnapshotManager::flags(const QnLayoutResourcePtr &resource) const {
    QHash<QnLayoutResourcePtr, Qn::LayoutFlags>::const_iterator pos = m_flagsByLayout.find(resource);
    if(pos == m_flagsByLayout.end()) {
        if(resource->resourcePool() == resourcePool()) {
            /* We didn't get the notification from resource pool yet. */
            return defaultFlags(resource);
        } else {
            qnWarning("Layout '%1' is not managed by this layout snapshot manager.", resource ? resource->getName() : QLatin1String("null"));
            return 0;
        }
    }

    return *pos;
}

void QnWorkbenchLayoutSnapshotManager::setFlags(const QnLayoutResourcePtr &resource, Qn::LayoutFlags flags) {
    QHash<QnLayoutResourcePtr, Qn::LayoutFlags>::iterator pos = m_flagsByLayout.find(resource);
    if(pos == m_flagsByLayout.end()) {
        pos = m_flagsByLayout.insert(resource, flags);
    } else if(*pos == flags) {
        return;
    }

    *pos = flags;
    
    emit flagsChanged(resource);
}

void QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot) {
    if(!resource) {
        qnNullWarning(resource);
        return;
    }

    QnLayoutResourceList resources;
    resources.push_back(resource);
    save(resources, object, slot);
}

void QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourceList &resources, QObject *object, const char *slot) {
    /* Submit all changes from workbench to resource. */
    foreach(const QnLayoutResourcePtr &resource, resources)
        if(QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource))
            synchronizer->submit();

    detail::QnWorkbenchLayoutReplyProcessor *processor = new detail::QnWorkbenchLayoutReplyProcessor(this, resources);
    connect(processor, SIGNAL(finished(int, const QByteArray &, const QnResourceList &, int)), object, slot);
    connection()->saveAsync(resources, processor, SLOT(at_finished(int, const QByteArray &, const QnResourceList &, int)));

    foreach(const QnLayoutResourcePtr &resource, resources)
        setFlags(resource, flags(resource) | Qn::LayoutIsBeingSaved);
}

void QnWorkbenchLayoutSnapshotManager::restore(const QnLayoutResourcePtr &resource) {
    if(!resource) {
        qnNullWarning(resource);
        return;
    }

    /* We don't want to get queued layout change signals for these changes, 
     * so there are no options but to disconnect before making them. */
    disconnectFrom(resource);
    {
        const QnWorkbenchLayoutSnapshot &snapshot = m_storage->snapshot(resource);
        resource->setItems(snapshot.items);
        resource->setName(snapshot.name);
    }
    connectTo(resource);

    setFlags(resource, flags(resource) & ~Qn::LayoutIsChanged);
}

void QnWorkbenchLayoutSnapshotManager::connectTo(const QnLayoutResourcePtr &resource) {
    connect(resource.data(),  SIGNAL(itemAdded(const QnLayoutItemData &)),      this,   SLOT(at_layout_changed()));
    connect(resource.data(),  SIGNAL(itemRemoved(const QnLayoutItemData &)),    this,   SLOT(at_layout_changed()));
    connect(resource.data(),  SIGNAL(itemChanged(const QnLayoutItemData &)),    this,   SLOT(at_layout_changed()));
    connect(resource.data(),  SIGNAL(nameChanged()),                            this,   SLOT(at_layout_changed()));
    connect(resource.data(),  SIGNAL(cellAspectRatioChanged()),                 this,   SLOT(at_layout_changed()));
    connect(resource.data(),  SIGNAL(cellSpacingChanged()),                     this,   SLOT(at_layout_changed()));
}

void QnWorkbenchLayoutSnapshotManager::disconnectFrom(const QnLayoutResourcePtr &resource) {
    disconnect(resource.data(), NULL, this, NULL);
}

Qn::LayoutFlags QnWorkbenchLayoutSnapshotManager::defaultFlags(const QnLayoutResourcePtr &resource) const {
    Qn::LayoutFlags result;

    if(resource->getId().isSpecial())
        result |= Qn::LayoutIsLocal;

    if((resource->flags() & QnResource::local_media) == QnResource::local_media)
        result |= Qn::LayoutIsFile;

    return result;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    /* Consider it saved by default. */
    m_storage->store(layoutResource);
    
    Qn::LayoutFlags flags = defaultFlags(layoutResource);
    setFlags(layoutResource, flags);

    layoutResource->setFlags(flags & Qn::LayoutIsLocal ? layoutResource->flags() & ~QnResource::remote : layoutResource->flags() | QnResource::remote); // TODO: this code does not belong here.
    
    /* Subscribe to changes to track changed status. */
    connectTo(layoutResource);
}

void QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    m_storage->remove(layoutResource);
    m_flagsByLayout.remove(layoutResource);

    disconnectFrom(layoutResource);
}

void QnWorkbenchLayoutSnapshotManager::at_layouts_saved(const QnLayoutResourceList &resources) {
    foreach(const QnLayoutResourcePtr &resource, resources) {
        m_storage->store(resource);

        setFlags(resource, 0); /* Not local, not being saved, not changed. */
    }
}

void QnWorkbenchLayoutSnapshotManager::at_layouts_saveFailed(const QnLayoutResourceList &resources) {
    foreach(const QnLayoutResourcePtr &resource, resources)
        setFlags(resource, flags(resource) & ~Qn::LayoutIsBeingSaved);
}

void QnWorkbenchLayoutSnapshotManager::at_layout_changed(const QnLayoutResourcePtr &resource) {
    setFlags(resource, flags(resource) | Qn::LayoutIsChanged);
}

void QnWorkbenchLayoutSnapshotManager::at_layout_changed() {
    QnLayoutResourcePtr resource = toSharedPointer(checked_cast<QnLayoutResource *>(sender()));
    at_layout_changed(resource);
}


