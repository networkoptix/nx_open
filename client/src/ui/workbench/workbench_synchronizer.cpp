#include "workbench_synchronizer.h"
#include <cassert>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/warnings.h>
#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include "workbench.h"
#include "workbench_layout.h"
#include "workbench_layout_synchronizer.h"
#include "workbench_context.h"

namespace {
    const char *qn_createdLocallyPropertyName = "_qn_createdLocally";

    void setCreatedLocally(QnWorkbenchLayout *layout, bool createdLocally) {
        layout->setProperty(qn_createdLocallyPropertyName, createdLocally);
    }

    bool isCreatedLocally(QnWorkbenchLayout *layout) {
        return layout->property(qn_createdLocallyPropertyName).toBool();
    }

} // anonymous namespace


void detail::WorkbenchSynchronizerReplyProcessor::at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int) {
    if(status == 0 && !m_synchronizer.isNull())
        m_synchronizer.data()->m_savedDataByResource[m_resource] = detail::LayoutData(m_resource);

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


QnWorkbenchSynchronizer::QnWorkbenchSynchronizer(QObject *parent):
    QObject(NULL),
    m_running(false),
    m_context(NULL),
    m_update(false),
    m_submit(false),
    m_connection(QnAppServerConnectionFactory::createConnection())
{}

QnWorkbenchSynchronizer::~QnWorkbenchSynchronizer() {}

void QnWorkbenchSynchronizer::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL) 
        stop();

    m_context = context;

    if(m_context != NULL) 
        start();
}

QnLayoutResourceList QnWorkbenchSynchronizer::poolLayoutResources() const {
    return QnResourceCriterion::filter<QnLayoutResource>(context()->resourcePool()->getResources());
}

QnLayoutResourcePtr QnWorkbenchSynchronizer::checkLayoutResource(QnWorkbenchLayout *layout) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return QnLayoutResourcePtr();
    }

    if(!m_running) {
        qnWarning("Synchronizer is not running.");
        return QnLayoutResourcePtr();
    }

    QnLayoutResourcePtr resource = layout->resource();
    if(resource.isNull()) {
        qnWarning("Given layout is not registered with workbench synchronizer.");
        return resource;
    }

    return resource;
}

void QnWorkbenchSynchronizer::save(QnWorkbenchLayout *layout, QObject *object, const char *slot) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return;

    /* Submit all changes to the resource. */
    QnWorkbenchLayoutSynchronizer::instance(layout)->submit();

    detail::WorkbenchSynchronizerReplyProcessor *processor = new detail::WorkbenchSynchronizerReplyProcessor(this, resource);
    connect(processor, SIGNAL(finished(int, const QByteArray &, const QnLayoutResourcePtr &)), object, slot);
    m_connection->saveAsync(resource, processor, SLOT(at_finished(int, const QByteArray &, QnResourceList, int)));
}

void QnWorkbenchSynchronizer::restore(QnWorkbenchLayout *layout) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return;
    
    resource->setItems(m_savedDataByResource[resource].items);
    resource->setName(m_savedDataByResource[resource].name);
}

bool QnWorkbenchSynchronizer::isChanged(const QnLayoutResourcePtr &resource) {
    detail::LayoutData &data = m_savedDataByResource[resource];
    return 
        resource->getItems() != data.items ||
        resource->getName() != data.name;
}

bool QnWorkbenchSynchronizer::isLocal(const QnLayoutResourcePtr &resource) {
    QnWorkbenchLayout *layout = QnWorkbenchLayout::layout(resource);
    if(layout == NULL)
        return false;
    
    return isLocal(layout);
}

bool QnWorkbenchSynchronizer::isChanged(QnWorkbenchLayout *layout) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return false;

    return isChanged(resource);
}

bool QnWorkbenchSynchronizer::isLocal(QnWorkbenchLayout *layout) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return false;

    return resource->getId().isSpecial(); //isCreatedLocally(layout); // TODO: this is not good, but we have no other options. Fix.
}

void QnWorkbenchSynchronizer::start() {
    assert(m_context != NULL);

    /* Clean workbench's layouts. */
    while(!context()->workbench()->layouts().isEmpty())
        delete context()->workbench()->layouts().back();

    /* Consider all pool's layouts saved. */
    foreach(const QnLayoutResourcePtr &resource, poolLayoutResources())
        m_savedDataByResource[resource] = detail::LayoutData(resource);

    /* Start listening to changes. */
    connect(context(),                  SIGNAL(aboutToBeDestroyed()),                   this,   SLOT(at_context_aboutToBeDestroyed()));
    connect(context()->resourcePool(),  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(context()->workbench(),     SIGNAL(layoutsChanged()),                       this,   SLOT(at_workbench_layoutsChanged()));

    m_submit = m_update = true;

    m_running = true;
    emit started();
}

void QnWorkbenchSynchronizer::stop() {
    assert(m_context != NULL);

    emit stopped();
    m_running = false;

    m_submit = m_update = false;

    /* Stop listening to changes. */
    disconnect(context(), NULL, this, NULL);
    disconnect(context()->resourcePool(), NULL, this, NULL);
    disconnect(context()->workbench(), NULL, this, NULL);

    /* Clean workbench's layouts. */
    while(!context()->workbench()->layouts().isEmpty())
        delete context()->workbench()->layouts().back();
}

void QnWorkbenchSynchronizer::update() {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QSet<QnLayoutResourcePtr> resources = poolLayoutResources().toSet();

    /* New layouts may have been added, but these are not on the workbench, 
     * so we don't need to do anything about them. 
     * 
     * Layouts may have been removed, and in this case we need to remove them
     * from the workbench too. */
    foreach(QnWorkbenchLayout *layout, context()->workbench()->layouts()) {
        QnLayoutResourcePtr resource = layout->resource();

        if(!resources.contains(resource)) { /* Corresponding layout resource was removed, remove layout. */
            m_savedDataByResource.remove(resource);
            delete layout;
        }
    }
}

void QnWorkbenchSynchronizer::submit() {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);

    /* Layout may have been closed, but it doesn't mean that we should remove it.
     *
     * New layout may have been added, and in this case we need to create a new
     * resource for it. */
    foreach(QnWorkbenchLayout *layout, context()->workbench()->layouts()) {
        QnLayoutResourcePtr resource = layout->resource();

        if(resource.isNull()) { 
            /* This actually is a newly created layout. */
            resource = QnLayoutResourcePtr(new QnLayoutResource());
            resource->setGuid(QUuid::createUuid());
            context()->resourcePool()->addResource(resource);
            if(context()->user())
                context()->user()->addLayout(resource); /* Leave layout a toplevel resource if there is no current user. */

            setCreatedLocally(layout, true);

            QnWorkbenchLayoutSynchronizer *synchronizer = new QnWorkbenchLayoutSynchronizer(layout, resource, this);
            synchronizer->setAutoDeleting(true);
            synchronizer->submit();

            /* Consider it saved in its present state. */
            m_savedDataByResource[resource] = detail::LayoutData(resource);
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchSynchronizer::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnWorkbenchSynchronizer::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    m_savedDataByResource.remove(layoutResource);

    /* Layout was removed, we need to remove it from the workbench too. */
    QnWorkbenchLayout *layout = QnWorkbenchLayout::layout(layoutResource);
    delete layout;
}

void QnWorkbenchSynchronizer::at_workbench_layoutsChanged() {
    submit();
}
