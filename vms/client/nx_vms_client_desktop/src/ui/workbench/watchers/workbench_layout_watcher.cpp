#include "workbench_layout_watcher.h"

#include <QtCore/QDir>

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/avi/avi_resource.h>

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

        if (!resource && !data.resource.uniqueId.isEmpty())
        {
            if (QDir::isAbsolutePath(data.resource.uniqueId))
            {
                /* Try to load local resource. */
                resource = QnResourcePtr(new QnAviResource(data.resource.uniqueId));
                resourcePool()->addResource(resource);
            }
        }

        if (resource)
        {
            data.resource.id = resource->getId();
            if (resource->hasFlags(Qn::local_media))
                data.resource.uniqueId = resource->getUniqueId();
            layout->updateItem(data);
        }
        else
        {
            layout->removeItem(data.uuid);
        }
    }
}
