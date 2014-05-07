#include "media_server_update_tool.h"

#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>
#include <utils/update/update_utils.h>
#include <utils/update_uploader.h>

#include <version.h>

namespace {

const qint64 maxUpdateFileSize = 100 * 1024 * 1024; // 100 MB
const QString QN_UPDATES_URL = lit("http://localhost:8000/updates");
const QString buildInformationSuffix(lit("update"));
const QString updatesDirName = lit(QN_PRODUCT_NAME_SHORT) + lit("_updates");
const QByteArray mutexName = "auto_update";
const int installationTimeout = 5 * 60 * 1000; // 5 minutes

QString updateFilePath(const QString &fileName) {
    QDir dir = QDir::temp();
    if (!dir.exists(updatesDirName))
        dir.mkdir(updatesDirName);
    dir.cd(updatesDirName);
    return dir.absoluteFilePath(fileName);
}

} // anonymous namespace

QnMediaServerUpdateTool::PeerUpdateInformation::PeerUpdateInformation(const QnMediaServerResourcePtr &server) :
    server(server),
    state(UpdateNotFound),
    updateInformation(0),
    progress(0)
{
    if (server)
        sourceVersion = server->getVersion();
}

ec2::AbstractECConnectionPtr QnMediaServerUpdateTool::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_state(Idle),
    m_checkResult(UpdateFound),
    m_onlineUpdateUrl(QN_UPDATES_URL),
    m_uploader(new QnUpdateUploader(this)),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
    connect(connection2()->getUpdatesManager().get(),   &ec2::AbstractUpdatesManager::updateChunkUploaded,   this,   &QnMediaServerUpdateTool::at_updateChunkUploaded);
    connect(m_uploader,                                 &QnUpdateUploader::finished,                    this,   &QnMediaServerUpdateTool::at_uploader_finished);
    connect(m_uploader,                                 &QnUpdateUploader::failed,                      this,   &QnMediaServerUpdateTool::at_uploader_failed);
    connect(m_uploader,                                 &QnUpdateUploader::progressChanged,             this,   &QnMediaServerUpdateTool::at_uploader_progressChanged);
    connect(m_uploader,                                 &QnUpdateUploader::peerProgressChanged,         this,   &QnMediaServerUpdateTool::peerProgressChanged);
    connect(ec2::QnDistributedMutexManager::instance(), &ec2::QnDistributedMutexManager::locked,        this,   &QnMediaServerUpdateTool::at_mutexLocked, Qt::QueuedConnection);
    connect(ec2::QnDistributedMutexManager::instance(), &ec2::QnDistributedMutexManager::lockTimeout,   this,   &QnMediaServerUpdateTool::at_mutexTimeout, Qt::QueuedConnection);
}

QnMediaServerUpdateTool::State QnMediaServerUpdateTool::state() const {
    return m_state;
}

bool QnMediaServerUpdateTool::isUpdating() const {
    return m_state >= DownloadingUpdate;
}

void QnMediaServerUpdateTool::setState(State state) {
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);
}

void QnMediaServerUpdateTool::setCheckResult(QnMediaServerUpdateTool::CheckResult result) {
    m_checkResult = result;
    setState(Idle);
}

void QnMediaServerUpdateTool::setUpdateResult(QnMediaServerUpdateTool::UpdateResult result) {
    m_updateResult = result;
    setState(Idle);
}

void QnMediaServerUpdateTool::setPeerState(const QnId &peerId, QnMediaServerUpdateTool::PeerUpdateInformation::State state) {
    auto it = m_updateInformationById.find(peerId);
    if (it == m_updateInformationById.end())
        return;

    if (it->state != state) {
        it->state = state;
        emit peerChanged(peerId);
    }
}

QnMediaServerUpdateTool::CheckResult QnMediaServerUpdateTool::updateCheckResult() const {
    return m_checkResult;
}

QnMediaServerUpdateTool::UpdateResult QnMediaServerUpdateTool::updateResult() const {
    return m_updateResult;
}

QnSoftwareVersion QnMediaServerUpdateTool::targetVersion() const {
    return m_targetVersion;
}

QnMediaServerUpdateTool::PeerUpdateInformation QnMediaServerUpdateTool::updateInformation(const QnId &peerId) const {
    auto it = m_updateInformationById.find(peerId);
    if (it != m_updateInformationById.end())
        return it.value();

    PeerUpdateInformation info(qnResPool->getResourceById(peerId).dynamicCast<QnMediaServerResource>());
    if (info.server && m_state == Idle) {
        info.updateInformation = m_updateFiles[info.server->getSystemInfo()];
        info.state = info.updateInformation ? PeerUpdateInformation::UpdateFound : PeerUpdateInformation::UpdateNotFound;
    }
    return info;
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

void QnMediaServerUpdateTool::checkOnlineUpdates(const QnSoftwareVersion &version) {
    setState(CheckingForUpdates);

    m_updateFiles.clear();
    m_updateInformationById.clear();

    m_targetMustBeNewer = version.isNull();
    m_targetVersion = version;

    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl(m_onlineUpdateUrl)));
    connect(reply, &QNetworkReply::finished, this, &QnMediaServerUpdateTool::at_updateReply_finished);
}

void QnMediaServerUpdateTool::checkLocalUpdates() {
    QRegExp updateFileRegExp(lit("update_.+_.+_\\d+\\.\\d+\\.\\d+\\.\\d+\\.zip"));

    setState(CheckingForUpdates);

    m_updateFiles.clear();
    m_updateInformationById.clear();

    QStringList entries = m_localUpdateDir.entryList(QStringList() << lit("*.zip"), QDir::Files);
    foreach (const QString &entry, entries) {
        if (!updateFileRegExp.exactMatch(entry))
            continue;

        QString fileName = m_localUpdateDir.absoluteFilePath(entry);
        QnSoftwareVersion version;
        QnSystemInformation sysInfo;

        if (!verifyUpdatePackage(fileName, &version, &sysInfo))
            continue;

        if (m_updateFiles.contains(sysInfo))
            continue;

        m_updateFiles.insert(sysInfo, UpdateFileInformationPtr(new UpdateFileInformation(version, fileName)));
    }

    checkUpdateCoverage();
}

void QnMediaServerUpdateTool::checkBuildOnline() {
    setState(CheckingForUpdates);
    m_targetMustBeNewer = false;

    QUrl url(m_updateLocationPrefix + m_targetVersion.toString() + lit("/") + buildInformationSuffix);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, &QnMediaServerUpdateTool::at_buildReply_finished);
}

void QnMediaServerUpdateTool::checkUpdateCoverage() {
    bool needUpdate = false;
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server)) {
        QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
        UpdateFileInformationPtr updateFileInformation = m_updateFiles[server->getSystemInfo()];
        if (!updateFileInformation) {
            setCheckResult(UpdateImpossible);
            return;
        }
        QnSoftwareVersion version = updateFileInformation->version;
        if ((m_targetMustBeNewer && version > server->getVersion()) || (!m_targetMustBeNewer && version != server->getVersion()))
            needUpdate = true;
    }

    setCheckResult(needUpdate ? UpdateFound : NoNewerVersion);
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
        setCheckResult(InternetProblem);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    map = map.value(lit(QN_CUSTOMIZATION_NAME)).toMap();
    QnSoftwareVersion latestVersion = QnSoftwareVersion(map.value(lit("latest_version")).toString());
    QString updatesPrefix = map.value(lit("updates_prefix")).toString();
    if (latestVersion.isNull() || updatesPrefix.isEmpty()) {
        setCheckResult(InternetProblem);
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
        setCheckResult((reply->error() == QNetworkReply::ContentNotFoundError) ? NoSuchBuild : InternetProblem);
        return;
    }

    QString urlPrefix = m_updateLocationPrefix + m_targetVersion.toString() + lit("/");

    QByteArray data = reply->readAll();
    QVariantMap platforms = QJsonDocument::fromJson(data).toVariant().toMap();
    for (auto platform = platforms.begin(); platform != platforms.end(); ++platform) {
        QVariantMap architectures = platform.value().toMap();
        for (auto arch = architectures.begin(); arch != architectures.end(); ++arch) {
            UpdateFileInformationPtr info(new UpdateFileInformation(m_targetVersion, QUrl(urlPrefix + arch.value().toString())));
            info->baseFileName = arch.value().toString();
            m_updateFiles.insert(QnSystemInformation(platform.key(), arch.key()), info);
        }
    }

    checkUpdateCoverage();
}

void QnMediaServerUpdateTool::updateServers() {
    // here we fix the list of servers to be updated

    m_idBySystemInformation.clear();
    m_downloadingUpdates.clear();
    m_updateInformationById.clear();

    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server)) {
        QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
        if (server->getStatus() != QnResource::Online)
            continue;

        PeerUpdateInformation info(server);
        info.updateInformation = m_updateFiles.value(server->getSystemInfo());

        if (!info.updateInformation) {
            Q_ASSERT_X(0, "No update info for server", Q_FUNC_INFO);
            return;
        }

        info.state = info.updateInformation->fileName.isEmpty() ? PeerUpdateInformation::PendingDownloading
                                                                : PeerUpdateInformation::PendingUpload;

        m_updateInformationById.insert(server->getId(), info);
        m_idBySystemInformation.insert(server->getSystemInfo(), server->getId());
    }

    setState(DownloadingUpdate);

    m_downloadingUpdates = QSet<QnSystemInformation>::fromList(m_updateFiles.keys());
    downloadNextUpdate();
}

bool QnMediaServerUpdateTool::cancelUpdate() {
    switch (m_state) {
    case DownloadingUpdate:
        m_downloadingUpdates.clear();
        break;
    case UploadingUpdate:
        m_uploader->cancel();
        break;
    case InstallingUpdate:
        return false;
    default:
        break;
    }

    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
        it->state = PeerUpdateInformation::UpdateCanceled;

    setState(Idle);
    return true;
}

void QnMediaServerUpdateTool::downloadNextUpdate() {
    if (m_state != DownloadingUpdate)
        return;

    auto it = m_downloadingUpdates.begin();

    if (it == m_downloadingUpdates.end()) {
        uploadUpdatesToServers();
        return;
    }

    foreach (const QnId &peerId, m_idBySystemInformation.values(*it))
        setPeerState(peerId, PeerUpdateInformation::UpdateDownloading);

    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl(m_updateFiles[*it]->url)));
    connect(reply,  &QNetworkReply::finished,           this,   &QnMediaServerUpdateTool::at_downloadReply_finished);
    connect(reply,  &QNetworkReply::downloadProgress,   this,   &QnMediaServerUpdateTool::at_downloadReply_downloadProgress);
}

void QnMediaServerUpdateTool::at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    int baseProgress = (m_updateFiles.size() - m_downloadingUpdates.size()) * 100 / m_updateFiles.size();
    baseProgress += (bytesReceived * (100 / m_updateFiles.size()) / bytesTotal);
    emit progressChanged(baseProgress);
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
        setUpdateResult(DownloadingFailed);
        return;
    }

    QnSystemInformation systemInformation = *m_downloadingUpdates.begin();
    UpdateFileInformationPtr info = m_updateFiles[systemInformation];

    QByteArray data = reply->readAll();
    QFile file(updateFilePath(info->baseFileName));
    if (!file.open(QFile::WriteOnly) || file.write(data) != data.size()) {
        setUpdateResult(DownloadingFailed);
        return;
    }
    file.close();

    info->fileName = file.fileName();
    m_downloadingUpdates.erase(m_downloadingUpdates.begin());

    foreach (const QnId &peerId, m_idBySystemInformation.values(systemInformation))
        setPeerState(peerId, PeerUpdateInformation::PendingUpload);

    downloadNextUpdate();
}

void QnMediaServerUpdateTool::uploadUpdatesToServers() {
    emit progressChanged(0);
    setState(UploadingUpdate);

    m_pendingUploadPeers = QSet<QnId>::fromList(m_updateInformationById.keys());
    m_pendingUploads = m_idBySystemInformation.uniqueKeys();

    uploadNextUpdate();
}

void QnMediaServerUpdateTool::uploadNextUpdate() {
    if (m_state != UploadingUpdate)
        return;

    if (m_pendingUploads.isEmpty()) {
        installUpdatesToServers();
        return;
    }

    QnSystemInformation sysInfo = m_pendingUploads.takeFirst();
    if (!m_uploader->uploadUpdate(m_targetVersion.toString(), m_updateFiles[sysInfo]->fileName, QSet<QnId>::fromList(m_idBySystemInformation.values(sysInfo))))
        setUpdateResult(UploadingFailed);
}

void QnMediaServerUpdateTool::at_updateChunkUploaded(const QString &updateId, const QnId &peerId, qint64 offset) {
    if (m_state != UploadingUpdate)
        return;

    if (updateId != m_targetVersion.toString())
        return;

    m_uploader->markChunkUploaded(peerId, offset);
}

void QnMediaServerUpdateTool::at_uploader_finished() {
    if (m_state != UploadingUpdate)
        return;

    foreach (const QnId &peerId, m_uploader->peers()) {
        m_pendingUploadPeers.remove(peerId);
        m_pendingInstallations.insert(peerId);
        setPeerState(peerId, PeerUpdateInformation::PendingInstallation);
    }

    uploadNextUpdate();
}

void QnMediaServerUpdateTool::at_uploader_failed() {
    foreach (const QnId &peerId, m_uploader->peers()) {
        setPeerState(peerId, PeerUpdateInformation::UpdateFailed);
        setUpdateResult(UploadingFailed);
    }
}

void QnMediaServerUpdateTool::at_uploader_progressChanged(int progress) {
    int peers = m_idBySystemInformation.size();
    int finished = peers - m_pendingUploadPeers.size();
    int current = m_uploader->peers().size();

    emit progressChanged((finished + current * progress / 100) * 100 / peers);
}

void QnMediaServerUpdateTool::installUpdatesToServers() {
    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
        it->state = PeerUpdateInformation::UpdateInstalling;

    setState(InstallingUpdate);

    m_distributedMutex = ec2::QnDistributedMutexManager::instance()->getLock(mutexName);
}

void QnMediaServerUpdateTool::at_mutexLocked(const QByteArray &) {
    connect(qnResPool, &QnResourcePool::resourceChanged, this, &QnMediaServerUpdateTool::at_resourceChanged);

    connection2()->getUpdatesManager()->installUpdate(m_targetVersion.toString(), m_pendingInstallations,
                                                      this, [this](int, ec2::ErrorCode){});
    m_distributedMutex->unlock();
    m_distributedMutex.clear();

    QTimer::singleShot(installationTimeout, this, SLOT(at_installationTimeout()));
}

void QnMediaServerUpdateTool::at_mutexTimeout(const QByteArray &) {
    m_distributedMutex.clear();
    setUpdateResult(InstallationFailed);
}

void QnMediaServerUpdateTool::at_resourceChanged(const QnResourcePtr &resource) {
    if (m_state != InstallingUpdate) {
       qWarning() << Q_FUNC_INFO << "We should not get here if we are not in InstallingUpdate state";
       return;
    }

    auto it = m_pendingInstallations.find(resource->getId());
    if (it == m_pendingInstallations.end())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    if (server->getVersion() == m_targetVersion) {
        m_pendingInstallations.erase(it);
        setPeerState(*it, PeerUpdateInformation::UpdateFinished);
    }

    if (m_pendingInstallations.isEmpty()) {
        disconnect(qnResPool, &QnResourcePool::resourceChanged, this, &QnMediaServerUpdateTool::at_resourceChanged);
        setUpdateResult(UpdateSuccessful);
    }
}

void QnMediaServerUpdateTool::at_installationTimeout() {
    // disconnect from resource pool and make final check
    disconnect(qnResPool, &QnResourcePool::resourceChanged, this, &QnMediaServerUpdateTool::at_resourceChanged);

    foreach (const QnId &id, m_pendingInstallations) {
        if (m_updateInformationById[id].server->getVersion() == m_targetVersion) {
            m_pendingInstallations.remove(id);
            setPeerState(id, PeerUpdateInformation::UpdateFinished);
        } else {
            setPeerState(id, PeerUpdateInformation::UpdateFailed);
        }
    }

    m_updateResult = m_pendingInstallations.isEmpty() ? UpdateSuccessful : InstallationFailed;
    m_pendingInstallations.clear();

    setState(Idle);
}
