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

    Q_GLOBAL_STATIC(detail::LayoutData, qn_dummyLayoutState);

} // anonymous namespace


void detail::WorkbenchSynchronizerReplyProcessor::at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int) {
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


QnWorkbenchSynchronizer::QnWorkbenchSynchronizer(QObject *parent):
    QObject(NULL),
    m_context(NULL),
    m_update(false),
    m_submit(false),
    m_connection(QnAppServerConnectionFactory::createConnection())
{}

QnWorkbenchSynchronizer::~QnWorkbenchSynchronizer() {
    setContext(NULL);
}

void QnWorkbenchSynchronizer::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL) 
        stop();

    m_context = context;

    if(m_context != NULL) 
        start();
}

void QnWorkbenchSynchronizer::start() {
    assert(m_context != NULL);

    /* Clean workbench's layouts. */
    while(!context()->workbench()->layouts().isEmpty())
        delete context()->workbench()->layouts().back();

    /* Start listening to changes. */
    connect(context(),                  SIGNAL(aboutToBeDestroyed()),                   this,   SLOT(at_context_aboutToBeDestroyed()));
    connect(context()->resourcePool(),  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(context()->workbench(),     SIGNAL(layoutsChanged()),                       this,   SLOT(at_workbench_layoutsChanged()));

    m_submit = m_update = true;
}

void QnWorkbenchSynchronizer::stop() {
    assert(m_context != NULL);

    m_submit = m_update = false;

    /* Stop listening to changes. */
    disconnect(context(), NULL, this, NULL);
    disconnect(context()->resourcePool(), NULL, this, NULL);
    disconnect(context()->workbench(), NULL, this, NULL);

    /* Clean workbench's layouts. */
    while(!context()->workbench()->layouts().isEmpty())
        delete context()->workbench()->layouts().back();
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
            setSavedState(resource, detail::LayoutData(resource));
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
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    /* Layout was removed, we need to remove it from the workbench too. */
    QnWorkbenchLayout *layout = QnWorkbenchLayout::layout(layoutResource);
    if(layout != NULL)
        delete layout;
}

void QnWorkbenchSynchronizer::at_workbench_layoutsChanged() {
    submit();
}
