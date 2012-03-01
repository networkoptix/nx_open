#include "workbench_layout_state_manager.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include "workbench_context.h"
#include "workbench_layout_snapshot_storage.h"
#include "workbench_layout_synchronizer.h"

// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutReplyProcessor
// -------------------------------------------------------------------------- //
void detail::QnWorkbenchLayoutReplyProcessor::at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int) {
    if(status == 0 && m_manager)
        m_manager.data()->at_layout_saved(m_resource);


    if(resources.size() != 1) {
        qnWarning("Received reply of invalid size %1: expected a list containing a single layout resource.", resources.size());
    } else {
        QnLayoutResourcePtr resource = resources[0].dynamicCast<QnLayoutResource>();
        if(!resource) {
            qnWarning("Received reply of invalid type: expected layout resource, got %1.", resources[0]->metaObject()->className());
        } else {
            m_resource->setId(resource->getId()); // TODO: is this needed? Try to remove.
        }
    }

    emit finished(status, errorString, m_resource);

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutSnapshotManager
// -------------------------------------------------------------------------- //
QnWorkbenchLayoutSnapshotManager::QnWorkbenchLayoutSnapshotManager(QObject *parent):    
    QObject(parent),
    m_context(NULL),
    m_storage(new QnWorkbenchLayoutSnapshotStorage(this)),
    m_connection(QnAppServerConnectionFactory::createConnection())
{}

QnWorkbenchLayoutSnapshotManager::~QnWorkbenchLayoutSnapshotManager() {
    setContext(NULL);
}

void QnWorkbenchLayoutSnapshotManager::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL)
        stop();

    m_context = context;

    if(m_context != NULL)
        start();
}

void QnWorkbenchLayoutSnapshotManager::start() {
    assert(m_context != NULL);


    /* Consider all pool's layouts saved. */
    foreach(const QnLayoutResourcePtr &resource, poolLayoutResources())
        m_storage->store(resource);

    /* Start listening to changes. */
    connect(context(),                  SIGNAL(aboutToBeDestroyed()),                   this,   SLOT(at_context_aboutToBeDestroyed()));
    connect(context()->resourcePool(),  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(context()->resourcePool(),  SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
}

void QnWorkbenchLayoutSnapshotManager::stop() {
    assert(m_context != NULL);

    disconnect(context(), NULL, this, NULL);

    m_storage->clear();
}

QnLayoutResourceList QnWorkbenchLayoutSnapshotManager::poolLayoutResources() const {
    return QnResourceCriterion::filter<QnLayoutResource>(context()->resourcePool()->getResources());
}

QnWorkbenchLayoutSnapshotManager::LayoutFlags QnWorkbenchLayoutSnapshotManager::flags(const QnLayoutResourcePtr &resource) const {
    QHash<QnLayoutResourcePtr, LayoutFlags>::const_iterator pos = m_flagsByLayout.find(resource);
    if(pos == m_flagsByLayout.end()) {
        qnWarning("Layout '%1' is not managed by this layout snapshot manager.", resource ? resource->getName() : QLatin1String("null"));
        return Local;
    }

    return *pos;
}

void QnWorkbenchLayoutSnapshotManager::setFlags(const QnLayoutResourcePtr &resource, LayoutFlags flags) {
    QHash<QnLayoutResourcePtr, LayoutFlags>::iterator pos = m_flagsByLayout.find(resource);
    if(*pos == flags)
        return;

    *pos = flags;
    
    emit flagsChanged(resource);
}

void QnWorkbenchLayoutSnapshotManager::save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot) {
    if(!resource) {
        qnNullWarning(resource);
        return;
    }

    /* Submit all changes from workbench to resource. */
    QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource);
    if(synchronizer)
        synchronizer->submit();

    detail::QnWorkbenchLayoutReplyProcessor *processor = new detail::QnWorkbenchLayoutReplyProcessor(this, resource);
    connect(processor, SIGNAL(finished(int, const QByteArray &, const QnLayoutResourcePtr &)), object, slot);
    m_connection->saveAsync(resource, processor, SLOT(at_finished(int, const QByteArray &, QnResourceList, int)));

    setFlags(resource, flags(resource) | BeingSaved);
}

void QnWorkbenchLayoutSnapshotManager::restore(const QnLayoutResourcePtr &resource) {
    if(!resource) {
        qnNullWarning(resource);
        return;
    }

    const QnWorkbenchLayoutSnapshot &state = m_storage->snapshot(resource);
    resource->setItems(state.items);
    resource->setName(state.name);

    setFlags(resource, flags(resource) & ~Changed);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutSnapshotManager::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    /* Consider it saved by default. */
    m_storage->store(layoutResource);

    setFlags(layoutResource, layoutResource->getId().isSpecial() ? Local : static_cast<LayoutFlags>(0));
}

void QnWorkbenchLayoutSnapshotManager::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    m_storage->remove(layoutResource);
    m_flagsByLayout.remove(layoutResource);
}

void QnWorkbenchLayoutSnapshotManager::at_layout_saved(const QnLayoutResourcePtr &resource) {
    m_storage->store(resource);

    setFlags(resource, 0); /* Not local, not being saved, not changed. */
}