#include "media_server_update_tool.h"

#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>

namespace {

const qint64 maxUpdateFileSize = 100 * 1024 * 1024; // 100 MB
const QString infoEntryName = lit("update.json");

} // anonymous namespace

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent)
{
    connect(connection2()->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateUploaded, this, &QnMediaServerUpdateTool::at_updateUploaded);
}

ec2::AbstractECConnectionPtr QnMediaServerUpdateTool::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnMediaServerUpdateTool::checkOnlineUpdates() {

}

void QnMediaServerUpdateTool::checkLocalUpdates() {
    QRegExp updateFileRegExp(lit("update_.+_.+_\\d+\\.\\d+\\.\\d+\\.\\d+\\.zip"));

    m_updates.clear();
    QStringList entries = m_localUpdateDir.entryList(QStringList() << lit("*.zip"), QDir::Files);
    foreach (const QString &entry, entries) {
        if (!updateFileRegExp.exactMatch(entry))
            continue;

        QString fileName = m_localUpdateDir.absoluteFilePath(entry);

        QuaZipFile infoFile(fileName, infoEntryName);
        if (!infoFile.open(QuaZipFile::ReadOnly))
            continue;

        QVariantMap info = QJsonDocument::fromJson(infoFile.readAll()).toVariant().toMap();
        if (info.isEmpty())
            continue;

        QnSystemInformation sysInfo(info.value(lit("platform")).toString(), info.value(lit("arch")).toString());
        if (!sysInfo.isValid())
            continue;

        QnSoftwareVersion version(info.value(lit("version")).toString());
        if (version.isNull())
            continue;

        if (!info.contains(lit("executable")))
            continue;

        m_updates.insert(sysInfo, UpdateInformation(version, fileName));
    }

    emit updatesListUpdated();
}

void QnMediaServerUpdateTool::updateServers() {
//    QFile file(m_updateFile);
//    if (!file.open(QFile::ReadOnly)) {
//        // TODO: #dklychkov error handling
//        return;
//    }

//    if (file.size() > maxUpdateFileSize) {
//        // TODO: #dklychkov error handling
//        return;
//    }

//    QString updateId = QFileInfo(file).fileName();
//    QByteArray data = file.readAll();

//    QnResourceList servers = qnResPool->getResourcesWithFlag(QnResource::server);

//    ec2::PeerList ids;
//    foreach (const QnResourcePtr &resource, servers)
//        ids.insert(resource->getId());

//    connection2()->getUpdatesManager()->sendUpdatePackage(updateId, data, ids,
    //                                                          this, [this](int reqID, ec2::ErrorCode errorCode) {});
}

void QnMediaServerUpdateTool::setUpdateMode(QnMediaServerUpdateTool::UpdateMode mode) {
    m_updateMode = mode;
}

void QnMediaServerUpdateTool::setLocalUpdateDir(const QDir &dir) {
    m_localUpdateDir = dir;
}

QHash<QnSystemInformation, QnMediaServerUpdateTool::UpdateInformation> QnMediaServerUpdateTool::availableUpdates() const {
    return m_updates;
}

void QnMediaServerUpdateTool::checkForUpdates() {
    if (m_updateMode == OnlineUpdate)
        checkOnlineUpdates();
    else
        checkLocalUpdates();
}

void QnMediaServerUpdateTool::at_updateUploaded(const QString &updateId, const QnId &peerId) {
    qDebug() << "uploaded!!!";
}
