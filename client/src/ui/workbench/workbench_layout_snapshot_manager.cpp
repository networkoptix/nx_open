#include "workbench_layout_snapshot_manager.h"

#include <cassert>

#include <api/app_server_connection.h>

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_storage.h>
#include <ui/workbench/workbench_layout_synchronizer.h>
#include <ui/workbench/workbench_layout.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutReplyProcessor
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutReplyProcessor::processReply( int handle, ec2::ErrorCode errorCode ) {
    /* Note that we may get reply of size 0 if Server is down.
     * This is why we use stored list of layouts. */
    if(m_manager)
        m_manager.data()->processReply((int)errorCode, m_resources, handle);

    emitFinished(this,
        (int)errorCode, //TODO: #ak need to check all dependent places and change int to ec2::ErrorCode.
                        //But for now it's ok, since both status and errorCode use 0 for success
        QnResourceList(m_resources), handle);
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
    connect(qnResPool,  &QnResourcePool::resourceRemoved, this,   &QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceRemoved);
    connect(qnResPool,  &QnResourcePool::resourceAdded,   this,   &QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceAdded);

    for(const QnLayoutResourcePtr &layout: qnResPool->getResources<QnLayoutResource>())
        at_resourcePool_resourceAdded(layout);
}

QnWorkbenchLayoutSnapshotManager::~QnWorkbenchLayoutSnapshotManager() {
    disconnect(qnResPool, nullptr, this, nullptr);

    for(const QnLayoutResourcePtr &layout: qnResPool->getResources<QnLayoutResource>())
        at_resourcePool_resourceRemoved(layout);

    m_storage->clear();
    m_flagsByLayout.clear();
}

bool QnWorkbenchLayoutSnapshotManager::isFile(const QnLayoutResourcePtr &resource) {
    return resource && (resource->flags() & Qn::url) && !resource->getUrl().isEmpty();
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

    if (flags.testFlag(Qn::ResourceIsBeingSaved))
        NX_ASSERT(accessController()->hasPermissions(resource, Qn::SavePermission), Q_FUNC_INFO, "Saving unsaveable resource");

    QHash<QnLayoutResourcePtr, Qn::ResourceSavingFlags>::iterator pos = m_flagsByLayout.find(resource);
    if(pos == m_flagsByLayout.end()) {
        pos = m_flagsByLayout.insert(resource, flags);
    } else if(*pos == flags) {
        return;
    }

    *pos = flags;

    emit flagsChanged(resource);
}

int QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot) {
    if(!resource) {
        qnNullWarning(resource);
        return -1;
    }

    QnLayoutResourceList resources;
    resources.push_back(resource);
    return save(resources, object, slot);
}

int QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourceList &resources, QObject *object, const char *slot) {
    QnWorkbenchLayoutReplyProcessor *processor = new QnWorkbenchLayoutReplyProcessor(this, resources);
    connect(processor, SIGNAL(finished(int, const QnResourceList &, int, const QString &)), object, slot);
    return save(resources, processor);
}

int QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourceList &resources, QnWorkbenchLayoutReplyProcessor *replyProcessor) {
    /* Submit all changes from workbench to resource. */
    foreach(const QnLayoutResourcePtr &resource, resources)
        if(QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource))
            synchronizer->submit();

    //TODO: #GDM SafeMode
    int handle = QnAppServerConnectionFactory::getConnection2()->getLayoutManager()->save(resources, replyProcessor, &QnWorkbenchLayoutReplyProcessor::processReply );

    foreach(const QnLayoutResourcePtr &resource, resources)
        setFlags(resource, flags(resource) | Qn::ResourceIsBeingSaved);
    return handle;
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
    connect(resource,  &QnResource::nameChanged,                    this,   &QnWorkbenchLayoutSnapshotManager::at_resource_changed);
    connect(resource,  &QnLayoutResource::itemAdded,                this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource,  &QnLayoutResource::itemRemoved,              this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource,  &QnLayoutResource::lockedChanged,            this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource,  &QnLayoutResource::cellAspectRatioChanged,   this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource,  &QnLayoutResource::cellSpacingChanged,       this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource,  &QnLayoutResource::backgroundImageChanged,   this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource,  &QnLayoutResource::backgroundSizeChanged,    this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);
    connect(resource,  &QnLayoutResource::backgroundOpacityChanged, this,   &QnWorkbenchLayoutSnapshotManager::at_layout_changed);

    /* This one is handled separately because it should not lead to marking layout as modified if layout is not opened right now. */
    connect(resource,  &QnLayoutResource::itemChanged,              this,   &QnWorkbenchLayoutSnapshotManager::at_layout_itemChanged);

    connect(resource,  &QnLayoutResource::storeRequested,           this,   &QnWorkbenchLayoutSnapshotManager::store);
}

void QnWorkbenchLayoutSnapshotManager::disconnectFrom(const QnLayoutResourcePtr &resource) {
    disconnect(resource, NULL, this, NULL);
}

Qn::ResourceSavingFlags QnWorkbenchLayoutSnapshotManager::defaultFlags(const QnLayoutResourcePtr &resource) const {
    Qn::ResourceSavingFlags result;

    if(resource->flags() & Qn::local)
        result |= Qn::ResourceIsLocal;

    return result;
}

void QnWorkbenchLayoutSnapshotManager::processReply(int status, const QnLayoutResourceList &resources, int handle) {
    Q_UNUSED(handle);

    if(status == 0) {
        foreach(const QnLayoutResourcePtr &resource, resources) {
            m_storage->store(resource);
            setFlags(resource, 0); /* Not local, not being saved, not changed. */
        }
    } else {
        foreach(const QnLayoutResourcePtr &resource, resources)
            setFlags(resource, flags(resource) & ~Qn::ResourceIsBeingSaved);
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

    layoutResource->setFlags(flags & Qn::ResourceIsLocal ? layoutResource->flags() & ~Qn::remote : layoutResource->flags() | Qn::remote); // TODO: #Elric this code does not belong here.

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

void QnWorkbenchLayoutSnapshotManager::at_layout_changed(const QnLayoutResourcePtr &resource) {
    setFlags(resource, flags(resource) | Qn::ResourceIsChanged);
}

void QnWorkbenchLayoutSnapshotManager::at_resource_changed(const QnResourcePtr &resource) {
    if(QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>())
        at_layout_changed(layoutResource);
}

void QnWorkbenchLayoutSnapshotManager::at_layout_itemChanged(const QnLayoutResourcePtr &resource)
{
    if (workbench()->currentLayout()->resource() == resource)
        at_layout_changed(resource);
}
