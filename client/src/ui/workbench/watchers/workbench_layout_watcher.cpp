#include "workbench_layout_watcher.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include "plugins/resource/avi/avi_resource.h"

#include <utils/common/warnings.h>

QnWorkbenchLayoutWatcher::QnWorkbenchLayoutWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}


QnWorkbenchLayoutWatcher::~QnWorkbenchLayoutWatcher() {
}

void QnWorkbenchLayoutWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(!layout)
        return;

    for (QnLayoutItemData data: layout->getItems()) {

        QnResourcePtr resource;
        /* Try to find online resource by id. */
        if (!data.resource.id.isNull()) {
            resource = qnResPool->getResourceById(data.resource.id);
        }
        else if (!data.resource.path.isEmpty()) {
            /* Try to find online resource by unique id. */
            resource = qnResPool->getResourceByUniqId(data.resource.path);
            if(!resource) {
                /* Try to load local resource. */
                resource = QnResourcePtr(new QnAviResource(data.resource.path));
                qnResPool->addResource(resource);
            }
        }

        if(resource) {
            data.resource.id = resource->getId();
            data.resource.path = resource->getUniqueId();
            layout->updateItem(data.uuid, data);
        } else {
            qnWarning("Invalid item with empty id and invalid path in layout '%1', item id %2 path %3.", layout->getName(), data.resource.id, data.resource.path);
            layout->removeItem(data.uuid);
        }
    }
}
