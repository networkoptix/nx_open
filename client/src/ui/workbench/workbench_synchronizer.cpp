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

QnWorkbenchSynchronizer::QnWorkbenchSynchronizer(QObject *parent):
    QObject(NULL),
    m_context(NULL),
    m_update(false),
    m_submit(false)
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

            QnWorkbenchLayoutSynchronizer *synchronizer = new QnWorkbenchLayoutSynchronizer(layout, resource, this);
            synchronizer->setAutoDeleting(true);
            synchronizer->submit();

            context()->resourcePool()->addResource(resource);
            if(context()->user())
                context()->user()->addLayout(resource); /* Leave layout a toplevel resource if there is no current user. */
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
