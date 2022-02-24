// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_watcher.h"

#include <QtCore/QDir>

#include <common/common_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>

QnWorkbenchLayoutWatcher::QnWorkbenchLayoutWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchLayoutWatcher::at_resourcePool_resourceAdded);

    for (const auto& resource: resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}


QnWorkbenchLayoutWatcher::~QnWorkbenchLayoutWatcher()
{
}

void QnWorkbenchLayoutWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    /* Quick check. */
    if (!resource->hasFlags(Qn::layout))
        return;

    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout);
    if (!layout)
        return;

    for (QnLayoutItemData data : layout->getItems())
    {
        QnResourcePtr resource = resourcePool()->getResourceByDescriptor(data.resource);

        if (!resource
            && !data.resource.path.isEmpty()
            && QFileInfo::exists(data.resource.path))
        {
            /* Try to load local resource. */
            resource = QnResourcePtr(
                new QnAviResource(data.resource.path, commonModule()->storagePluginFactory()));
            resourcePool()->addResource(resource);
        }

        if (resource)
        {
            data.resource.id = resource->getId();
            data.resource.path = nx::vms::client::desktop::layout::resourcePath(resource);
            layout->updateItem(data);
        }
        else
        {
            layout->removeItem(data.uuid);
        }
    }
}
