#include "workbench_layout_manager.h"
#include <cassert>
#include "workbench_context.h"

QnWorkbenchLayoutManager::QnWorkbenchLayoutManager(QObject *parent):    
    QObject(parent)
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
        setSavedState(resource, detail::LayoutData(resource));

    /* Start listening to changes. */
    connect(context(),                  SIGNAL(aboutToBeDestroyed()),                   this,   SLOT(at_context_aboutToBeDestroyed()));
    connect(context()->resourcePool(),  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(context()->resourcePool(),  SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
}

void QnWorkbenchLayoutManager::stop() {
    assert(m_context != NULL);

    disconnect(context(), NULL, this, NULL);
}


const detail::LayoutData &QnWorkbenchSynchronizer::savedState(const QnLayoutResourcePtr &resource) {
    QHash<QnLayoutResourcePtr, detail::LayoutData>::const_iterator pos = m_savedDataByResource.find(resource);
    if(pos == m_savedDataByResource.end()) {
        qnWarning("No saved state exists for layout '%1'.", resource ? resource->getName() : QLatin1String("null"));
        return *qn_dummyLayoutState();
    }

    return *pos;
}

void QnWorkbenchSynchronizer::setSavedState(const QnLayoutResourcePtr &resource, const detail::LayoutData &state) {
    m_savedDataByResource[resource] = state;
}

void QnWorkbenchSynchronizer::removeSavedState(const QnLayoutResourcePtr &resource) {
    m_savedDataByResource.remove(resource);
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

    detail::WorkbenchSynchronizerReplyProcessor *processor = new detail::WorkbenchSynchronizerReplyProcessor(this, resource);
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
