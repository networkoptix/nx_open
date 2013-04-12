#include "workbench_layout_snapshot_manager.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <core/resource/layout_resource.h>
#include <core/resource_managment/resource_pool.h>
#include "workbench_context.h"
#include "workbench_layout_snapshot_storage.h"
#include "workbench_layout_synchronizer.h"
#include "workbench_layout.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"

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
        setFlags(resource, flags(resource) | Qn::ResourceIsBeingSaved);
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
    }
    connectTo(resource);

    setFlags(resource, flags(resource) & ~Qn::ResourceIsChanged);
}

void QnWorkbenchLayoutSnapshotManager::connectTo(const QnLayoutResourcePtr &resource) {
    connect(resource.data(),  SIGNAL(itemAdded(const QnLayoutResourcePtr &, const QnLayoutItemData &)),     this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(itemRemoved(const QnLayoutResourcePtr &, const QnLayoutItemData &)),   this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(itemChanged(const QnLayoutResourcePtr &, const QnLayoutItemData &)),   this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(nameChanged(const QnResourcePtr &)),                                   this,   SLOT(at_layout_changed(const QnResourcePtr &)));
    connect(resource.data(),  SIGNAL(cellAspectRatioChanged(const QnLayoutResourcePtr &)),                  this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
    connect(resource.data(),  SIGNAL(cellSpacingChanged(const QnLayoutResourcePtr &)),                      this,   SLOT(at_layout_changed(const QnLayoutResourcePtr &)));
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


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    //todo #Sasha: moved from resourcePool. check it
    foreach(QnLayoutItemData data, layoutResource->getItems()) 
    {
        if(!data.resource.id.isValid()) {
            QnResourcePtr resource = qnResPool->getResourceByUniqId(data.resource.path);
            if(!resource) {
                if(data.resource.path.isEmpty()) {
                    qnWarning("Invalid item with empty id and path in layout '%1'.", layoutResource->getName());
                } else {
                    resource = QnResourcePtr(new QnAviResource(data.resource.path));
                    qnResPool->addResource(resource);
                }
            }

            if(resource) {
                data.resource.id = resource->getId();
                layoutResource->updateItem(data.uuid, data);
            }
        } else if(data.resource.path.isEmpty()) {
            QnResourcePtr resource = qnResPool->getResourceById(data.resource.id);
            if(!resource) {
                qnWarning("Invalid resource id '%1'.", data.resource.id.toString());
            } else {
                data.resource.path = resource->getUniqueId();
                layoutResource->updateItem(data.uuid, data);
            }
        }
    }


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

void QnWorkbenchLayoutSnapshotManager::at_layouts_saved(const QnLayoutResourceList &resources) {
    foreach(const QnLayoutResourcePtr &resource, resources) {
        m_storage->store(resource);

        setFlags(resource, 0); /* Not local, not being saved, not changed. */
    }
}

void QnWorkbenchLayoutSnapshotManager::at_layouts_saveFailed(const QnLayoutResourceList &resources) {
    foreach(const QnLayoutResourcePtr &resource, resources)
        setFlags(resource, flags(resource) & ~Qn::ResourceIsBeingSaved);
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
