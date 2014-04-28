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
const QString updatesDirName = lit(QN_PRODUCT_NAME_SHORT) + lit("_updates");
const QByteArray mutexName = "auto_update";


QString updateFilePath(const QString &fileName) {
    QDir dir = QDir::temp();
    if (!dir.exists(updatesDirName))
        dir.mkdir(updatesDirName);
    dir.cd(updatesDirName);
    return dir.absoluteFilePath(fileName);
}

} // anonymous namespace

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_state(Idle),
    m_checkResult(UpdateFound),
    m_onlineUpdateUrl(QN_UPDATES_URL),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
    connect(connection2()->getUpdatesManager().get(),   &ec2::AbstractUpdatesManager::updateUploaded,   this,   &QnMediaServerUpdateTool::at_updateUploaded);
    connect(ec2::QnDistributedMutexManager::instance(), &ec2::QnDistributedMutexManager::locked,        this,   &QnMediaServerUpdateTool::at_mutexLocked, Qt::QueuedConnection);
    connect(ec2::QnDistributedMutexManager::instance(), &ec2::QnDistributedMutexManager::lockTimeout,   this,   &QnMediaServerUpdateTool::at_mutexTimeout, Qt::QueuedConnection);
}

QnMediaServerUpdateTool::State QnMediaServerUpdateTool::state() const {
    return m_state;
}

QnMediaServerUpdateTool::CheckResult QnMediaServerUpdateTool::updateCheckResult() const {
    return m_checkResult;
}

ec2::AbstractECConnectionPtr QnMediaServerUpdateTool::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnMediaServerUpdateTool::checkOnlineUpdates(const QnSoftwareVersion &version) {
    setState(CheckingForUpdates);
    m_updates.clear();
    m_targetMustBeNewer = version.isNull();
    m_targetVersion = version;
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl(m_onlineUpdateUrl)));
    connect(reply, &QNetworkReply::finished, this, &QnMediaServerUpdateTool::at_updateReply_finished);
}

void QnMediaServerUpdateTool::checkLocalUpdates() {
    QRegExp updateFileRegExp(lit("update_.+_.+_\\d+\\.\\d+\\.\\d+\\.\\d+\\.zip"));

    setState(CheckingForUpdates);

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

    checkUpdateCoverage();
}

void QnMediaServerUpdateTool::checkUpdateCoverage() {
    bool needUpdate = false;
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server)) {
        QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
        QnSoftwareVersion version = m_updates.value(server->getSystemInfo()).version;
        if (version.isNull()) {
            m_checkResult = UpdateImpossible;
            setState(Idle);
            return;
        }
        if ((m_targetMustBeNewer && version > server->getVersion()) || (!m_targetMustBeNewer && version != server->getVersion()))
            needUpdate = true;
    }

    m_checkResult = needUpdate ? UpdateFound : NoNewerVersion;
    setState(Idle);

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
        m_checkResult = InternetProblem;
        setState(Idle);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    map = map.value(lit(QN_CUSTOMIZATION_NAME)).toMap();
    QnSoftwareVersion latestVersion = QnSoftwareVersion(map.value(lit("latest_version")).toString());
    QString updatesPrefix = map.value(lit("updates_prefix")).toString();
    if (latestVersion.isNull() || updatesPrefix.isEmpty()) {
        m_checkResult = InternetProblem;
        setState(Idle);
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
        m_checkResult = (reply->error() == QNetworkReply::ContentNotFoundError) ? NoSuchBuild : InternetProblem;
        setState(Idle);
        return;
    }

    QString urlPrefix = m_updateLocationPrefix + m_targetVersion.toString() + lit("/");

    QByteArray data = reply->readAll();
    QVariantMap platforms = QJsonDocument::fromJson(data).toVariant().toMap();
    for (auto platform = platforms.begin(); platform != platforms.end(); ++platform) {
        QVariantMap architectures = platform.value().toMap();
        for (auto arch = architectures.begin(); arch != architectures.end(); ++arch) {
            UpdateInformation info(m_targetVersion, QUrl(urlPrefix + arch.value().toString()));
            info.baseFileName = arch.value().toString();
            m_updates.insert(QnSystemInformation(platform.key(), arch.key()), info);
        }
    }

    checkUpdateCoverage();
}

void QnMediaServerUpdateTool::at_downloadReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (m_state != DownloadingUpdate)
        return;

    if (reply->error() != QNetworkReply::NoError) {
        m_updateResult = DownloadingFailed;
        setState(Idle);
        return;
    }

    auto it = m_downloadingUpdates.begin();

    QByteArray data = reply->readAll();
    QFile file(updateFilePath(it.value().baseFileName));
    if (!file.open(QFile::WriteOnly) || file.write(data) != data.size()) {
        m_updateResult = DownloadingFailed;
        setState(Idle);
        return;
    }
    file.close();
    it.value().fileName = file.fileName();
    m_updates.insert(it.key(), it.value());
    m_downloadingUpdates.erase(it);

    downloadNextUpdate();
}

void QnMediaServerUpdateTool::updateServers() {
    setState(DownloadingUpdate);
    m_downloadingUpdates = m_updates;
    downloadNextUpdate();
}

QHash<QnSystemInformation, QnMediaServerUpdateTool::UpdateInformation> QnMediaServerUpdateTool::availableUpdates() const {
    return m_updates;
}

QnSoftwareVersion QnMediaServerUpdateTool::targetVersion() const {
    return m_targetVersion;
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

void QnMediaServerUpdateTool::cancelUpdate() {
    switch (m_state) {
    case DownloadingUpdate:
        m_downloadingUpdates.clear();
        break;
    case UploadingUpdate:
        break;
    default:
        break;
    }
    setState(Idle);
}

void QnMediaServerUpdateTool::at_updateUploaded(const QString &updateId, const QnId &peerId) {
    if (updateId != m_targetVersion.toString())
        return;

    m_pendingInstallServers.insert(peerId, m_pendingUploadServers.take(peerId));

    emit progressChanged(m_pendingInstallServers.size() * 100 / (m_pendingInstallServers.size() + m_pendingUploadServers.size()));

    if (m_pendingUploadServers.isEmpty())
        installUpdatesToServers();
}

void QnMediaServerUpdateTool::at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    int baseProgress = (m_updates.size() - m_downloadingUpdates.size()) * 100 / m_updates.size();
    baseProgress += (bytesReceived * (100 / m_updates.size()) / bytesTotal);
    emit progressChanged(baseProgress);
}

void QnMediaServerUpdateTool::downloadNextUpdate() {
    auto it = m_downloadingUpdates.begin();

    while (it != m_downloadingUpdates.end() && !it.value().fileName.isEmpty())
        it = m_downloadingUpdates.erase(it);

    if (it == m_downloadingUpdates.end()) {
        uploadUpdatesToServers();
        return;
    }

    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl(it.value().url)));
    connect(reply,  &QNetworkReply::finished,           this,   &QnMediaServerUpdateTool::at_downloadReply_finished);
    connect(reply,  &QNetworkReply::downloadProgress,   this,   &QnMediaServerUpdateTool::at_downloadReply_downloadProgress);
}

void QnMediaServerUpdateTool::at_mutexLocked(const QByteArray &) {
    connection2()->getUpdatesManager()->installUpdate(m_targetVersion.toString(), ec2::PeerList::fromList(m_pendingInstallServers.keys()),
                                                      this, [this](int reqId, ec2::ErrorCode errorCode){});
    m_distributedMutex->unlock();
    m_distributedMutex.clear();
}

void QnMediaServerUpdateTool::at_mutexTimeout(const QByteArray &) {
    m_distributedMutex.clear();
    m_updateResult = InstallationFailed;
    setState(Idle);
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

void QnMediaServerUpdateTool::uploadUpdatesToServers() {
    setState(UploadingUpdate);

    m_pendingUploadServers.clear();
    m_serverIdBySystemInformation.clear();
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server)) {
        QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
        if (server->getStatus() == QnResource::Offline)
            continue;

        m_pendingUploadServers.insert(server->getId(), server);
        m_serverIdBySystemInformation.insert(server->getSystemInfo(), server->getId());
    }

    foreach (const QnSystemInformation &info, m_serverIdBySystemInformation.keys()) {
        if (!m_updates.contains(info)) {
            m_updateResult = UploadingFailed;
            setState(Idle);
            return;
        }

        const UpdateInformation &updateInfo = m_updates[info];
        QFile file(updateInfo.fileName);
        if (!file.open(QFile::ReadOnly)) {
            m_updateResult = UploadingFailed;
            setState(Idle);
            return;
        }

        QByteArray updateData = file.readAll();

        ec2::PeerList peers;
        foreach (const QnId &id, m_serverIdBySystemInformation.values(info))
            peers.insert(id);

        connection2()->getUpdatesManager()->sendUpdatePackage(m_targetVersion.toString(), updateData, peers,
                                                              this, [this](int reqId, ec2::ErrorCode errorCode){});
    }
}

void QnMediaServerUpdateTool::installUpdatesToServers() {
    setState(InstallingUpdate);

    m_distributedMutex = ec2::QnDistributedMutexManager::instance()->getLock(mutexName);
}
