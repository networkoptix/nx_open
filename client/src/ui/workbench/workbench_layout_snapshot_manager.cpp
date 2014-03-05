#include "workbench_layout_snapshot_manager.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include "workbench_context.h"
#include "workbench_layout_snapshot_storage.h"
#include "workbench_layout_synchronizer.h"
#include "workbench_layout.h"

// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutReplyProcessor
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutReplyProcessor::processReply(int status, const QnResourceList &resources, int handle) {
    QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();

    /* Note that we may get reply of size 0 if EC is down.
     * This is why we use stored list of layouts. */
    if(m_manager)
        m_manager.data()->processSaveLayoutsReply(status, m_layouts, layouts);

    emitFinished(this, status, resources, handle);

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutSnapshotManager
// -------------------------------------------------------------------------- //
QnWorkbenchLayoutSnapshotManager::QnWorkbenchLayoutSnapshotManager(QObject *parent):    
    base_type(parent),
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

bool QnWorkbenchLayoutSnapshotManager::isFile(const QnLayoutResourcePtr &resource) {
    return resource && (resource->flags() & QnResource::url) && !resource->getUrl().isEmpty();
}

Qn::ResourceSavingFlags QnWorkbenchLayoutSnapshotManager::flags(const QnLayoutResourcePtr &resource) const {
    if(!resource) {
        qnNullWarning(resource);
        return Qn::ResourceIsLocal;
    }

    QHash<QnLayoutResourcePtr, Qn::ResourceSavingFlags>::const_iterator pos = m_flagsByLayout.find(resource);
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

Qn::ResourceSavingFlags QnWorkbenchLayoutSnapshotManager::flags(QnWorkbenchLayout *layout) const {
    if(!layout) {
        qnNullWarning(layout);
        return Qn::ResourceIsLocal;
    }

    QnLayoutResourcePtr resource = layout->resource();
    if(!resource)
        return Qn::ResourceIsLocal;

    return flags(resource);
}

void QnWorkbenchLayoutSnapshotManager::setFlags(const QnLayoutResourcePtr &resource, Qn::ResourceSavingFlags flags) {
    QHash<QnLayoutResourcePtr, Qn::ResourceSavingFlags>::iterator pos = m_flagsByLayout.find(resource);
    if(pos == m_flagsByLayout.end()) {
        pos = m_flagsByLayout.insert(resource, flags);
    } else if(*pos == flags) {
        return;
    }

    *pos = flags;
    
    emit flagsChanged(resource);
}

void QnWorkbenchLayoutSnapshotManager::setFlags(QnWorkbenchLayout *layout, Qn::ResourceSavingFlags flags) {
    if(!layout) {
        qnNullWarning(layout);
        return;
    }

    QnLayoutResourcePtr resource = layout->resource();
    if(!resource)
        return;

    setFlags(resource, flags);
}

int QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot) {
    if(!resource) {
        qnNullWarning(resource);
        return 0;
    }

    QnLayoutResourceList resources;
    resources.push_back(resource);
    return save(resources, object, slot);
}

int QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourceList &resources, QObject *object, const char *slot) {
    /* Submit all changes from workbench to resource. */
    foreach(const QnLayoutResourcePtr &resource, resources)
        if(QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource))
            synchronizer->submit();

    QnWorkbenchLayoutReplyProcessor *processor = new QnWorkbenchLayoutReplyProcessor(this, resources);
    if (object && slot)
        processor->connect(SIGNAL(finished(int, const QnResourceList &, int)), object, slot);
    int result = connection()->saveAsync(resources, processor, SLOT(processReply(int, const QnResourceList &, int)));

    foreach(const QnLayoutResourcePtr &resource, resources)
        setFlags(resource, flags(resource) | Qn::ResourceIsBeingSaved);
    return result;
}

void QnWorkbenchLayoutSnapshotManager::store(const QnLayoutResourcePtr &resource) {
    if(!resource) {
        qnNullWarning(resource);
        return;
    }

    /* We don't want to get queued layout change signals that are not yet delivered, 
     * so there are no options but to disconnect. */
    disconnectFrom(resource);
    {
        m_storage->store(resource);
    }
    connectTo(resource);

    setFlags(resource, flags(resource) & ~Qn::ResourceIsChanged);
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
        resource->setCellAspectRatio(snapshot.cellAspectRatio);
        resource->setCellSpacing(snapshot.cellSpacing);
        resource->setBackgroundSize(snapshot.backgroundSize);
        resource->setBackgroundImageFilename(snapshot.backgroundImageFilename);
        resource->setBackgroundOpacity(snapshot.backgroundOpacity);
        resource->setLocked(snapshot.locked);
    }
    connectTo(resource);

    if(QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource))
        synchronizer->reset();

    setFlags(resource, flags(resource) & ~Qn::ResourceIsChanged);
}

void QnWorkbenchLayoutSnapshotManager::connectTo(const QnLayoutResourcePtr &resource) {
    connect(resource.data(),  SIGNAL(itemAdded(const QnLayoutResourcePtr &, const QnLayoutItemData &)),     this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &)),   this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(itemChanged(const QnLayoutResourcePtr &, const QnLayoutItemData &)),   this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(nameChanged(const QnResourcePtr &)),                                   this,   SLOT(at_layout_changed(const QnResourcePtr &)));
    connect(resource.data(),  SIGNAL(lockedChanged(const QnLayoutResourcePtr &)),                           this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(cellAspectRatioChanged(const QnLayoutResourcePtr &)),                  this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(cellSpacingChanged(const QnLayoutResourcePtr &)),                      this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(backgroundImageChanged(const QnLayoutResourcePtr &)),                  this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(backgroundSizeChanged(const QnLayoutResourcePtr &)),                   this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(backgroundOpacityChanged(const QnLayoutResourcePtr &)),                this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(userCanEditChanged(const QnLayoutResourcePtr &)),                      this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(storeRequested(const QnLayoutResourcePtr &)),                          this,   SLOT(at_layout_storeRequested(const QnLayoutResourcePtr &)));
}

void QnWorkbenchLayoutSnapshotManager::disconnectFrom(const QnLayoutResourcePtr &resource) {
    disconnect(resource.data(), NULL, this, NULL);
}

Qn::ResourceSavingFlags QnWorkbenchLayoutSnapshotManager::defaultFlags(const QnLayoutResourcePtr &resource) const {
    Qn::ResourceSavingFlags result;

    if(resource->getId().isSpecial())
        result |= Qn::ResourceIsLocal;

    return result;
}

void QnWorkbenchLayoutSnapshotManager::processSaveLayoutsReply(int status, const QnLayoutResourceList &localLayouts, const QnLayoutResourceList &receivedLayouts) {
    if(status == 0) {
        foreach (const QnLayoutResourcePtr &newLayout, receivedLayouts) {
            QnResourcePtr resource = qnResPool->getResourceByGuid(newLayout->getGuid());
            QnLayoutResourcePtr existingLayout = resource ? resource.dynamicCast<QnLayoutResource>() : QnLayoutResourcePtr();
            if (existingLayout)
                existingLayout->setId(newLayout->getId());
        }

        foreach(const QnLayoutResourcePtr &layout, localLayouts) {
            m_storage->store(layout);
            setFlags(layout, 0); /* Not local, not being saved, not changed. */
        }
    } else {
        foreach(const QnLayoutResourcePtr &layout, localLayouts)
            setFlags(layout, flags(layout) & ~Qn::ResourceIsBeingSaved);
    }
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
    
    Qn::ResourceSavingFlags flags = defaultFlags(layoutResource);
    setFlags(layoutResource, flags);

    layoutResource->setFlags(flags & Qn::ResourceIsLocal ? layoutResource->flags() & ~QnResource::remote : layoutResource->flags() | QnResource::remote); // TODO: #Elric this code does not belong here.
    
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

void QnWorkbenchLayoutSnapshotManager::at_layout_storeRequested(const QnLayoutResourcePtr &resource) {
    store(resource);
}

void QnWorkbenchLayoutSnapshotManager::at_layout_changed(const QnLayoutResourcePtr &resource) {
    setFlags(resource, flags(resource) | Qn::ResourceIsChanged);
}

void QnWorkbenchLayoutSnapshotManager::at_layout_changed(const QnResourcePtr &resource) {
    if(QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>())
        at_layout_changed(layoutResource);
}
