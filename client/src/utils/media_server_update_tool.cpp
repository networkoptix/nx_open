#include "media_server_update_tool.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent)
{
}

QString QnMediaServerUpdateTool::updateFile() const {
    return m_updateFile;
}

void QnMediaServerUpdateTool::setUpdateFile(const QString &fileName) {
    m_updateFile = fileName;
}

void QnMediaServerUpdateTool::updateServers() {
    QnResourceList servers = qnResPool->getResourcesWithFlag(QnResource::server);

    foreach (const QnResourcePtr &resource, servers) {
        QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
        m_pendingUploadServers.insert(server->getId().toString(), server);
        server->apiConnection()->uploadUpdateAsync(m_updateFile, this, SLOT(updateUploaded(int,QString,int)));
    }
}

void QnMediaServerUpdateTool::updateUploaded(int status, const QString &reply, int handle) {

}
