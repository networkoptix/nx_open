#include "workbench_system_name_watcher.h"

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "api/app_server_connection.h"

#include "common/common_module.h"

QnWorkbenchSystemNameWatcher::QnWorkbenchSystemNameWatcher(QObject *parent) :
    QObject(parent)
{
    connect(qnResPool, &QnResourcePool::resourceChanged, this, &QnWorkbenchSystemNameWatcher::at_resourceChanged);
}

QnWorkbenchSystemNameWatcher::~QnWorkbenchSystemNameWatcher() {}

void QnWorkbenchSystemNameWatcher::at_resourceChanged(const QnResourcePtr &resource) {
    if (QnId(QnAppServerConnectionFactory::getConnection2()->connectionInfo().ecsGuid) != resource->getId())
        return;

    qnCommon->setLocalSystemName(resource.staticCast<QnMediaServerResource>()->getSystemName());
}
