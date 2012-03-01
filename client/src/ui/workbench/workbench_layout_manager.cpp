#include "workbench_layout_manager.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <core/resource/layout_resource.h>
#include "workbench_context.h"

// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutState
// -------------------------------------------------------------------------- //
class QnWorkbenchLayoutState {
public:
    QnWorkbenchLayoutState() {}

    QnWorkbenchLayoutState(const QnLayoutResourcePtr &resource):
        items(resource->getItems()),
        name(resource->getName())
    {}

    QnLayoutItemDataMap items;
    QString name;
};


// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutStateStorage
// -------------------------------------------------------------------------- //
class QnWorkbenchLayoutStateStorage: public QObject {
public:
    QnWorkbenchLayoutStateStorage(QObject *parent = NULL): QObject(parent) {}

    const QnWorkbenchLayoutState &state(const QnLayoutResourcePtr &resource) const {
        QHash<QnLayoutResourcePtr, QnWorkbenchLayoutState>::const_iterator pos = m_stateByLayout.find(resource);
        if(pos == m_stateByLayout.end()) {
            qnWarning("No saved state exists for layout '%1'.", resource ? resource->getName() : QLatin1String("null"));
            return m_emptyState;
        }

        return *pos;
    }

    void store(const QnLayoutResourcePtr &resource) {
        m_stateByLayout[resource] = QnWorkbenchLayoutState(resource);
    }

    void remove(const QnLayoutResourcePtr &resource) {
        m_stateByLayout.remove(resource);
    }

    void clear() {
        m_stateByLayout.clear();
    }

private:
    QHash<QnLayoutResourcePtr, QnWorkbenchLayoutState> m_stateByLayout;
    QnWorkbenchLayoutState m_emptyState;
};


// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutReplyProcessor
// -------------------------------------------------------------------------- //
void detail::QnWorkbenchLayoutReplyProcessor::at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int) {
    if(status == 0 && !m_synchronizer.isNull())
        m_synchronizer.data()->setSavedState(m_resource, detail::LayoutData(m_resource));

    if(resources.size() != 1) {
        qnWarning("Received reply of invalid size %1: expected a list containing a single layout resource.", resources.size());
    } else {
        QnLayoutResourcePtr resource = resources[0].dynamicCast<QnLayoutResource>();
        if(!resource) {
            qnWarning("Received reply of invalid type: expected layout resource, got %1.", resources[0]->metaObject()->className());
        } else {
            m_resource->setId(resource->getId());
        }
    }

    emit finished(status, errorString, m_resource);

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchLayoutManager
// -------------------------------------------------------------------------- //
QnWorkbenchLayoutManager::QnWorkbenchLayoutManager(QObject *parent):    
    QObject(parent),
    m_context(NULL),
    m_stateStorage(new QnWorkbenchLayoutStateStorage(this))
{

}

QnWorkbenchLayoutManager::~QnWorkbenchLayoutManager() {
    setContext(NULL);
}

void QnWorkbenchLayoutManager::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL)
        stop();

    m_context = context;

    if(m_context != NULL)
        start();
}

void QnWorkbenchLayoutManager::start() {
    assert(m_context != NULL);


    /* Consider all pool's layouts saved. */
    foreach(const QnLayoutResourcePtr &resource, poolLayoutResources())
        m_stateStorage->store(resource);

    /* Start listening to changes. */
    connect(context(),                  SIGNAL(aboutToBeDestroyed()),                   this,   SLOT(at_context_aboutToBeDestroyed()));
    connect(context()->resourcePool(),  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(context()->resourcePool(),  SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
}

void QnWorkbenchLayoutManager::stop() {
    assert(m_context != NULL);

    disconnect(context(), NULL, this, NULL);

    m_stateStorage->clear();
}


QnLayoutResourceList QnWorkbenchSynchronizer::poolLayoutResources() const {
    return QnResourceCriterion::filter<QnLayoutResource>(context()->resourcePool()->getResources());
}

void QnWorkbenchSynchronizer::save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot) {
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
}

void QnWorkbenchSynchronizer::restore(const QnLayoutResourcePtr &resource) {
    if(!resource) {
        qnNullWarning(resource);
        return;
    }

    const detail::LayoutData &state = savedState(resource);
    resource->setItems(state.items);
    resource->setName(state.name);
}

bool QnWorkbenchSynchronizer::isChanged(const QnLayoutResourcePtr &resource) {
    if(!resource) {
        qnNullWarning(resource);
        return false;
    }

    const detail::LayoutData &data = savedState(resource);
    return 
        resource->getItems() != data.items ||
        resource->getName() != data.name;
}

bool QnWorkbenchSynchronizer::isLocal(const QnLayoutResourcePtr &resource) {
    if(!resource) {
        qnNullWarning(resource);
        return true;
    }

    return resource->getId().isSpecial(); // TODO: this is not good, but we have no other options. Fix?
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchLayoutManager::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnWorkbenchSynchronizer::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    setSavedState(layoutResource, detail::LayoutData(layoutResource));
}

void QnWorkbenchSynchronizer::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    removeSavedState(layoutResource);

    /* Layout was removed, we need to remove it from the workbench too. */
    QnWorkbenchLayout *layout = QnWorkbenchLayout::layout(layoutResource);
    if(layout != NULL)
        delete layout;
}
