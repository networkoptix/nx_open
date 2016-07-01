#include "workbench_layout_watcher.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include "plugins/resource/avi/avi_resource.h"

#include <utils/common/warnings.h>

QnWorkbenchLayoutWatcher::QnWorkbenchLayoutWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnResPool, SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);
}


QnWorkbenchLayoutWatcher::~QnWorkbenchLayoutWatcher() {
}

void QnWorkbenchLayoutWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(!layout)
        return;

    for (QnLayoutItemData data: layout->getItems())
    {
        QnResourcePtr resource = qnResPool->getResourceByDescriptor(data.resource);

        if (!resource && !data.resource.uniqueId.isEmpty())
        {
                /* Try to load local resource. */
            resource = QnResourcePtr(new QnAviResource(data.resource.uniqueId));
            qnResPool->addResource(resource);
        }

        if (resource)
        {
            data.resource.id = resource->getId();
            data.resource.uniqueId = resource->getUniqueId();
            layout->updateItem(data.uuid, data);
        }
        else
        {
            qnWarning("Invalid item with empty id and invalid path in layout '%1', item path %2.", layout->getName(), data.resource.uniqueId);
            layout->removeItem(data.uuid);
        }
    }
}
