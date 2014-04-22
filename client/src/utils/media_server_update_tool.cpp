#include "media_server_update_tool.h"

#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>

#include <version.h>

namespace {

const qint64 maxUpdateFileSize = 100 * 1024 * 1024; // 100 MB
const QString infoEntryName = lit("update.json");
const QString QN_UPDATES_URL = lit("http://localhost:8000/updates");
const QString buildInformationSuffix(lit("update"));

} // anonymous namespace

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_state(Idle),
    m_onlineUpdateUrl(QN_UPDATES_URL),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
    connect(connection2()->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateUploaded, this, &QnMediaServerUpdateTool::at_updateUploaded);
}

QnMediaServerUpdateTool::State QnMediaServerUpdateTool::state() const {
    return m_state;
}

ec2::AbstractECConnectionPtr QnMediaServerUpdateTool::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnMediaServerUpdateTool::checkOnlineUpdates(const QnSoftwareVersion &version) {
    setState(CheckingForUpdates);
    m_targetMustBeNewer = version.isNull();
    m_targetVersion = version;
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl(m_onlineUpdateUrl)));
    connect(reply, &QNetworkReply::finished, this, &QnMediaServerUpdateTool::at_updateReply_finished);
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

    setState(m_updates.isEmpty() ? Idle : UpdateFound);
}

void QnMediaServerUpdateTool::checkUpdateCoverage() {
    bool needUpdate = false;
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server)) {
        QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
        QnSoftwareVersion version = m_updates.value(server->getSystemInfo()).version;
        if (version.isNull()) {
            setState(UpdateImpossible);
            return;
        }
        if ((m_targetMustBeNewer && version > server->getVersion()) || (!m_targetMustBeNewer && version != server->getVersion()))
            needUpdate = true;
    }

    if (m_targetMustBeNewer && !needUpdate)
        setState(Idle);
    else
        setState(UpdateFound);

    return;
}

void QnMediaServerUpdateTool::at_updateReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setState(CheckingFailed);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    map = map.value(lit(QN_CUSTOMIZATION_NAME)).toMap();
    QnSoftwareVersion latestVersion = QnSoftwareVersion(map.value(lit("latest_version")).toString());
    QString updatesPrefix = map.value(lit("updates_prefix")).toString();
    if (latestVersion.isNull() || updatesPrefix.isEmpty()) {
        setState(CheckingFailed);
        return;
    }

    if (m_targetVersion.isNull())
        m_targetVersion = latestVersion;
    m_updateLocationPrefix = updatesPrefix;

    checkBuildOnline();
}

void QnMediaServerUpdateTool::at_buildReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setState(CheckingFailed);
        return;
    }

    QString urlPrefix = m_updateLocationPrefix + m_targetVersion.toString() + lit("/");

    m_updates.clear();
    QByteArray data = reply->readAll();
    QVariantMap platforms = QJsonDocument::fromJson(data).toVariant().toMap();
    for (auto platform = platforms.begin(); platform != platforms.end(); ++platform) {
        QVariantMap architectures = platform.value().toMap();
        for (auto arch = architectures.begin(); arch != architectures.end(); ++arch)
            m_updates.insert(QnSystemInformation(platform.key(), arch.key()), UpdateInformation(m_targetVersion, urlPrefix + arch.value().toString()));
    }

    checkUpdateCoverage();
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

QHash<QnSystemInformation, QnMediaServerUpdateTool::UpdateInformation> QnMediaServerUpdateTool::availableUpdates() const {
    return m_updates;
}

void QnMediaServerUpdateTool::checkForUpdates() {
    if (m_state >= CheckingForUpdates)
        return;

    checkOnlineUpdates();
}

void QnMediaServerUpdateTool::checkForUpdates(const QnSoftwareVersion &version) {
    if (m_state >= CheckingForUpdates)
        return;

    checkOnlineUpdates(version);
}

void QnMediaServerUpdateTool::checkForUpdates(const QString &path) {
    if (m_state >= CheckingForUpdates)
        return;

    m_localUpdateDir = QDir(path);
    checkLocalUpdates();
}

void QnMediaServerUpdateTool::at_updateUploaded(const QString &updateId, const QnId &peerId) {
    qDebug() << "uploaded!!!";
}

void QnMediaServerUpdateTool::setState(State state) {
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);
}

void QnMediaServerUpdateTool::checkBuildOnline() {
    setState(CheckingForUpdates);
    m_targetMustBeNewer = false;

    QUrl url(m_updateLocationPrefix + m_targetVersion.toString() + lit("/") + buildInformationSuffix);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, &QnMediaServerUpdateTool::at_buildReply_finished);
}
