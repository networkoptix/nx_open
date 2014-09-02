#include "workbench_synchronizer.h"
#include <cassert>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/warnings.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include "workbench.h"
#include "workbench_layout.h"
#include "workbench_layout_synchronizer.h"
#include "workbench_context.h"

QnWorkbenchSynchronizer::QnWorkbenchSynchronizer(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_submit(false),
    m_update(false)
{
    /* Clean workbench's layouts. */
    while(!workbench()->layouts().isEmpty())
        delete workbench()->layouts().back();

    /* Start listening to changes. */
    connect(resourcePool(),  &QnResourcePool::resourceRemoved,  this,   &QnWorkbenchSynchronizer::at_resourcePool_resourceRemoved);
    connect(workbench(),     &QnWorkbench::layoutsChanged,      this,   &QnWorkbenchSynchronizer::at_workbench_layoutsChanged);

    m_submit = m_update = true;
}

QnWorkbenchSynchronizer::~QnWorkbenchSynchronizer() {
    m_submit = m_update = false;

    /* Stop listening to changes. */
    disconnect(resourcePool(), NULL, this, NULL);
    disconnect(workbench(), NULL, this, NULL);

    /* Clean workbench's layouts. */
    while(!workbench()->layouts().isEmpty())
        delete workbench()->layouts().back();
}

void QnWorkbenchSynchronizer::submit() {
    if(!m_submit)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_update, false);

    /* Layout may have been closed, but it doesn't mean that we should remove it.
     *
     * New layout may have been added, and in this case we need to create a new
     * resource for it. */
    foreach(QnWorkbenchLayout *layout, workbench()->layouts()) {
        QnLayoutResourcePtr resource = layout->resource();

        if(resource.isNull()) { 
            /* This actually is a newly created layout. */
            resource = QnLayoutResourcePtr(new QnLayoutResource());
            resource->setId(QUuid::createUuid());
            resource->setTypeByName(lit("Layout"));
            resource->addFlags(Qn::local); // TODO: #Elric #EC2

            QnWorkbenchLayoutSynchronizer *synchronizer = new QnWorkbenchLayoutSynchronizer(layout, resource, this);
            synchronizer->setAutoDeleting(true);
            synchronizer->submit();

            if(context()->user()) {
                resource->setParentId(context()->user()->getId());
                resource->setUserCanEdit(true);
            }

            resourcePool()->addResource(resource);
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchSynchronizer::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if(!m_update)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_submit, false);

    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    /* Layout was removed, we need to remove it from the workbench too. */
    QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(layoutResource);
    if(layout != NULL)
        delete layout;
}

void QnWorkbenchSynchronizer::at_workbench_layoutsChanged() {
    submit();
}
