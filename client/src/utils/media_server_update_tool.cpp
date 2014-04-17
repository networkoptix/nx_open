#include "media_server_update_tool.h"

#include <QtCore/QFileInfo>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>

namespace {

const qint64 maxUpdateFileSize = 100 * 1024 * 1024; // 100 MB

} // anonymous namespace

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent)
{
}

ec2::AbstractECConnectionPtr QnMediaServerUpdateTool::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

QString QnMediaServerUpdateTool::updateFile() const {
    return m_updateFile;
}

void QnMediaServerUpdateTool::setUpdateFile(const QString &fileName) {
    m_updateFile = fileName;
}

void QnMediaServerUpdateTool::updateServers() {
    QFile file(m_updateFile);
    if (!file.open(QFile::ReadOnly)) {
        // TODO: #dklychkov error handling
        return;
    }

    if (file.size() > maxUpdateFileSize) {
        // TODO: #dklychkov error handling
        return;
    }

    QString updateId = QFileInfo(file).fileName();
    QByteArray data = file.readAll();

    QnResourceList servers = qnResPool->getResourcesWithFlag(QnResource::server);

//    connection2()->getUpdatesManager()->sendUpdate

//    foreach (const QnResourcePtr &resource, servers) {
//        QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
//        m_pendingUploadServers.insert(server->getId().toString(), server);
//        server->apiConnection()->uploadUpdateAsync(updateId, data, this, SLOT(updateUploaded(int,QString,int)));
//    }
}

void QnMediaServerUpdateTool::updateUploaded(int status, const QString &reply, int handle) {

}
