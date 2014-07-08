#include "workbench_layout_watcher.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include "plugins/resource/avi/avi_resource.h"

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

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
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return;

    foreach(QnLayoutItemData data, layoutResource->getItems())
    {
        if(data.resource.id.isNull()) {
            QnResourcePtr resource = qnResPool->getResourceByUniqId(data.resource.path);
            if(!resource) {
                if(data.resource.path.isEmpty()) {
                    qnWarning("Invalid item with empty id and path in layout '%1'.", layoutResource->getName());
                } else {
                    resource = QnResourcePtr(new QnAviResource(data.resource.path));
                    qnResPool->addResource(resource);
                }
            }

            if(resource) {
                data.resource.id = resource->getId();
                layoutResource->updateItem(data.uuid, data);
            }
        } else if(data.resource.path.isEmpty()) {
            QnResourcePtr resource = qnResPool->getResourceById(data.resource.id);
            if(!resource) {
                qnWarning("Invalid resource id '%1'.", data.resource.id.toString());
            } else {
                data.resource.path = resource->getUniqueId();
                layoutResource->updateItem(data.uuid, data);
            }
        }
    }
}
