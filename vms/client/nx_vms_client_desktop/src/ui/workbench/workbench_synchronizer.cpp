// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_synchronizer.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
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
    connect(workbench(), &QnWorkbench::layoutsChanged, this,
        &QnWorkbenchSynchronizer::at_workbench_layoutsChanged);

    m_submit = m_update = true;
}

QnWorkbenchSynchronizer::~QnWorkbenchSynchronizer() {
    m_submit = m_update = false;

    /* Stop listening to changes. */
    disconnect(resourcePool(), nullptr, this, nullptr);
    disconnect(workbench(), nullptr, this, nullptr);

    /* Clean workbench's layouts. */
    while(!workbench()->layouts().isEmpty())
        delete workbench()->layouts().back();
}

void QnWorkbenchSynchronizer::submit()
{
    if (!m_submit)
        return;

    QScopedValueRollback<bool> guard(m_update, false);

    /* Layout may have been closed, but it doesn't mean that we should remove it.
     *
     * New layout may have been added, and in this case we need to create a new
     * resource for it. */
    foreach(QnWorkbenchLayout *layout, workbench()->layouts())
    {
        QnLayoutResourcePtr resource = layout->resource();

        if (resource.isNull())
        {
            /* This actually is a newly created layout. */
            resource = QnLayoutResourcePtr(new QnLayoutResource());
            resource->setIdUnsafe(QnUuid::createUuid());
            resource->addFlags(Qn::local);

            QnWorkbenchLayoutSynchronizer *synchronizer = new QnWorkbenchLayoutSynchronizer(layout, resource, this);
            synchronizer->setAutoDeleting(true);
            synchronizer->submit();

            if (context()->user())
                resource->setParentId(context()->user()->getId());

            resourcePool()->addResource(resource);
        }
    }
}

void QnWorkbenchSynchronizer::at_workbench_layoutsChanged() {
    submit();
}
