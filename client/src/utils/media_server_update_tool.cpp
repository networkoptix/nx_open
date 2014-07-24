#include "media_server_update_tool.h"

#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>
#include <QtCore/QFutureWatcher>
#include <QtConcurrent/qtconcurrentrun.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <utils/update/update_utils.h>
#include <utils/check_update_peer_task.h>
#include <utils/download_updates_peer_task.h>
#include <utils/upload_updates_peer_task.h>
#include <utils/install_updates_peer_task.h>
#include <utils/rest_update_peer_task.h>
#include <utils/applauncher_utils.h>

#include <version.h>

namespace {

const QString QN_UPDATES_URL = lit("http://enk.me/bg/dklychkov/exmaple_update/updates");
const QString QN_UPDATE_PACKAGE_URL = lit("http://enk.me/bg/dklychkov/exmaple_update/get_update");
const QString updatesDirName = lit(QN_PRODUCT_NAME_SHORT) + lit("_updates");
const QString mutexName = lit("auto_update");

#ifdef Q_OS_MACX
const bool defaultDisableClientUpdates = true;
#else
const bool defaultDisableClientUpdates = false;
#endif

bool verifyFile(const QString &fileName, qint64 size, const QString &md5) {
    QFile file(fileName);

    if (!file.exists() || file.size() != size)
        return false;

    if (!md5.isEmpty() && makeMd5(&file) != md5)
        return false;

    return true;
}

} // anonymous namespace

QnMediaServerUpdateTool::PeerUpdateInformation::PeerUpdateInformation(const QnMediaServerResourcePtr &server) :
    server(server),
    state(UpdateUnknown),
    updateInformation(0),
    progress(0)
{
    if (server)
        sourceVersion = server->getVersion();
}

// TODO: #dklychkov Split this class to a set of QnNetworkPeerTask

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_tasksThread(new QThread(this)),
    m_state(Idle),
    m_checkResult(UpdateFound),
    m_denyMajorUpdates(false),
    m_distributedMutex(0),
    m_checkForUpdatesPeerTask(new QnCheckForUpdatesPeerTask(this)),
    m_downloadUpdatesPeerTask(new QnDownloadUpdatesPeerTask(this)),
    m_uploadUpdatesPeerTask(new QnUploadUpdatesPeerTask(this)),
    m_installUpdatesPeerTask(new QnInstallUpdatesPeerTask(this)),
    m_restUpdatePeerTask(new QnRestUpdatePeerTask(this)),
    m_clientRequiresInstaller(false),
    m_disableClientUpdates(defaultDisableClientUpdates)
{
    m_checkForUpdatesPeerTask->setUpdatesUrl(QN_UPDATES_URL);

    m_checkForUpdatesPeerTask->moveToThread(m_tasksThread);
    m_downloadUpdatesPeerTask->moveToThread(m_tasksThread);
    m_uploadUpdatesPeerTask->moveToThread(m_tasksThread);
    m_installUpdatesPeerTask->moveToThread(m_tasksThread);
    m_restUpdatePeerTask->moveToThread(m_tasksThread);

    connect(m_checkForUpdatesPeerTask,                  &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_checkForUpdatesTask_finished);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_downloadTask_finished);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_uploadTask_finished);
    connect(m_installUpdatesPeerTask,                   &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_installTask_finished);
    connect(m_restUpdatePeerTask,                       &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_restUpdateTask_finished);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_downloadTask_peerFinished);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_uploadTask_peerFinished);
    connect(m_installUpdatesPeerTask,                   &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_installTask_peerFinished);
    connect(m_restUpdatePeerTask,                       &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_restUpdateTask_peerFinished);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::progressChanged,            this,   &QnMediaServerUpdateTool::progressChanged);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::progressChanged,            this,   &QnMediaServerUpdateTool::progressChanged);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::peerProgressChanged,        this,   &QnMediaServerUpdateTool::at_networkTask_peerProgressChanged);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::peerProgressChanged,        this,   &QnMediaServerUpdateTool::at_networkTask_peerProgressChanged);
}

QnMediaServerUpdateTool::~QnMediaServerUpdateTool() {
    if (m_tasksThread->isRunning()) {
        m_tasksThread->quit();
        m_tasksThread->wait();
    }
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

    switch (result) {
    case UpdateFound:
        m_resultString = tr("Update has been successfully finished.");
        break;
    case InternetProblem:
        m_resultString = tr("Check for updates failed.");
        break;
    case NoNewerVersion:
        m_resultString = tr("All component in your system are already up to date.");
        break;
    case NoSuchBuild:
        m_resultString = tr("There is no such build on the update server");
        break;
    case UpdateImpossible:
        if (m_clientUpdateFile.isNull())
            m_resultString = tr("Cannot start update.\nAn update for the client was not found.");
        else
            m_resultString = tr("Cannot start update.\nAn update for one or more servers was not found.");
        break;
    case BadUpdateFile:
        m_resultString = tr("Cannot update from this file:\n%1").arg(QFileInfo(m_checkForUpdatesPeerTask->updateFileName()).fileName());
        break;
    }

    setState(Idle);
}

void QnMediaServerUpdateTool::setUpdateResult(QnMediaServerUpdateTool::UpdateResult result) {
    m_updateResult = result;

    switch (result) {
    case UpdateSuccessful:
        m_resultString = tr("Update has been successfully finished.");
        break;
    case Cancelled:
        m_resultString = tr("Update has been cancelled.");
        break;
    case LockFailed:
        m_resultString = tr("Someone has already started an update.");
        break;
    case DownloadingFailed:
        m_resultString = tr("Could not download updates.");
        break;
    case UploadingFailed:
        m_resultString = tr("Could not upload updates to servers.");
        break;
    case ClientInstallationFailed:
        m_resultString = tr("Could not install an pdate to the client.");
        break;
    case InstallationFailed:
    case RestInstallationFailed:
        m_resultString = tr("Could not install updates on one or more servers.");
        break;
    }

    setState(Idle);
}

void QnMediaServerUpdateTool::finishUpdate(QnMediaServerUpdateTool::UpdateResult result) {
    m_tasksThread->quit();
    unlockMutex();
    removeTemporaryDir();
    setUpdateResult(result);
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

QString QnMediaServerUpdateTool::resultString() const {
    return m_resultString;
}

QnSoftwareVersion QnMediaServerUpdateTool::targetVersion() const {
    return m_targetVersion;
}

void QnMediaServerUpdateTool::setDenyMajorUpdates(bool denyMajorUpdates) {
    m_denyMajorUpdates = denyMajorUpdates;
}

QnMediaServerUpdateTool::PeerUpdateInformation QnMediaServerUpdateTool::updateInformation(const QnId &peerId) const {
    auto it = m_updateInformationById.find(peerId);
    if (it != m_updateInformationById.end())
        return it.value();

    PeerUpdateInformation info(qnResPool->getIncompatibleResourceById(peerId, true).dynamicCast<QnMediaServerResource>());
    if (info.server && m_state == Idle) {
        info.updateInformation = m_updateFiles[info.server->getSystemInfo()];
        if (m_targetVersion.isNull())
            info.state = PeerUpdateInformation::UpdateUnknown;
        else
            info.state = info.updateInformation ? PeerUpdateInformation::UpdateFound : PeerUpdateInformation::UpdateNotFound;
    }
    return info;
}

QnMediaServerResourceList QnMediaServerUpdateTool::targets() const {
    return m_targets;
}

void QnMediaServerUpdateTool::setTargets(const QSet<QnId> &targets, bool client) {
    m_targets.clear();

    foreach (const QnId &id, targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;
        m_targets.append(server);
    }

    m_disableClientUpdates = !client;
}

QnMediaServerResourceList QnMediaServerUpdateTool::actualTargets() const {
    QnMediaServerResourceList result;

    if (m_targets.isEmpty()) {
        foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server))
            result.append(resource.staticCast<QnMediaServerResource>());

        foreach (const QnResourcePtr &resource, qnResPool->getAllIncompatibleResources()) {
            if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>()) {
                if (server->getSystemName() == qnCommon->localSystemName())
                    result.append(server);
            }
        }

    } else {
        result = m_targets;
    }

    return result;
}

QUrl QnMediaServerUpdateTool::generateUpdatePackageUrl() const {
    QUrlQuery query;

    QString versionSuffix;
    if (m_targetVersion.isNull()) {
        versionSuffix = lit("/latest");
        query.addQueryItem(lit("current"), qnCommon->engineVersion().toString());
    } else {
        versionSuffix = QString(lit("/%1-%2")).arg(m_targetVersion.toString(), passwordForBuild((unsigned)m_targetVersion.build()));
    }

    QSet<QnSystemInformation> systemInformationList;

    if (m_idBySystemInformation.isEmpty()) {
        foreach (const QnMediaServerResourcePtr &server, actualTargets())
            systemInformationList.insert(server->getSystemInfo());
    } else {
        systemInformationList = QSet<QnSystemInformation>::fromList(m_idBySystemInformation.keys());
    }

    query.addQueryItem(lit("client"), QnSystemInformation::currentSystemInformation().toString().replace(QLatin1Char(' '), QLatin1Char('_')));
    foreach (const QnSystemInformation &systemInformation, systemInformationList)
        query.addQueryItem(lit("server"), systemInformation.toString().replace(QLatin1Char(' '), QLatin1Char('_')));

    QUrl url(QN_UPDATE_PACKAGE_URL + versionSuffix);
    url.setQuery(query);

    return url;
}

void QnMediaServerUpdateTool::reset() {
    if (m_state != Idle)
        return;

    m_updateInformationById.clear();
    m_updateFiles.clear();
    m_clientUpdateFile.clear();
    m_targetVersion = QnSoftwareVersion();
    m_clientRequiresInstaller = false;
    m_disableClientUpdates = defaultDisableClientUpdates;
}

bool QnMediaServerUpdateTool::isClientRequiresInstaller() const {
    return m_clientRequiresInstaller;
}

void QnMediaServerUpdateTool::checkForUpdates(const QnSoftwareVersion &version) {
    if (m_state >= CheckingForUpdates)
        return;

    setState(CheckingForUpdates);

    QnIdSet peers;
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        peers.insert(server->getId());

    m_checkForUpdatesPeerTask->setPeers(peers);
    m_checkForUpdatesPeerTask->setTargetVersion(version);
    m_checkForUpdatesPeerTask->setDisableClientUpdates(m_disableClientUpdates);
    m_tasksThread->start();
    QMetaObject::invokeMethod(m_checkForUpdatesPeerTask, "start", Qt::QueuedConnection);
}

void QnMediaServerUpdateTool::checkForUpdates(const QString &fileName) {
    if (m_state >= CheckingForUpdates)
        return;

    setState(CheckingForUpdates);

    QnIdSet peers;
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        peers.insert(server->getId());

    m_checkForUpdatesPeerTask->setPeers(peers);
    m_checkForUpdatesPeerTask->setUpdateFileName(fileName);
    m_checkForUpdatesPeerTask->setDisableClientUpdates(m_disableClientUpdates);
    m_tasksThread->start();
    QMetaObject::invokeMethod(m_checkForUpdatesPeerTask, "start", Qt::QueuedConnection);
}

void QnMediaServerUpdateTool::removeTemporaryDir() {
    if (m_localTemporaryDir.isEmpty())
        return;

    QDir(m_localTemporaryDir).removeRecursively();
    m_localTemporaryDir.clear();
}

void QnMediaServerUpdateTool::updateServers() {
    m_updateId = QUuid::createUuid().toString();
    m_updateInformationById.clear();
    m_idBySystemInformation.clear();

    foreach (const QnMediaServerResourcePtr &server, actualTargets()) {
        bool incompatible = (server->getStatus() == QnResource::Incompatible);

        if (server->getStatus() != QnResource::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        if (!m_checkForUpdatesPeerTask->needUpdate(server->getVersion(), m_targetVersion))
            continue;

        QnId peerId = server->getId();

        m_targetPeerIds.insert(peerId);
        if (incompatible)
            m_incompatiblePeerIds.insert(peerId);

        PeerUpdateInformation info(server);
        info.updateInformation = m_updateFiles.value(server->getSystemInfo());

        if (!info.updateInformation) {
            Q_ASSERT_X(0, "No update info for server", Q_FUNC_INFO);
            return;
        }

        info.state = info.updateInformation->fileName.isEmpty() ? PeerUpdateInformation::PendingDownloading
                                                                : PeerUpdateInformation::PendingUpload;

        m_updateInformationById.insert(peerId, info);
        m_idBySystemInformation.insert(server->getSystemInfo(), peerId);
    }

    setState(DownloadingUpdate);

    m_tasksThread->start();

    downloadUpdates();
}

bool QnMediaServerUpdateTool::cancelUpdate() {
    switch (m_state) {
    case DownloadingUpdate:
        m_downloadUpdatesPeerTask->cancel();
        break;
    case InstallingToIncompatiblePeers:
        m_restUpdatePeerTask->cancel();
        break;
    case UploadingUpdate:
        m_uploadUpdatesPeerTask->cancel();
        break;
    case InstallingUpdate:
        return false;
    default:
        break;
    }

    unlockMutex();

    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
        it->state = PeerUpdateInformation::UpdateCanceled;

    setState(Idle);
    return true;
}

void QnMediaServerUpdateTool::downloadUpdates() {
    QHash<QUrl, QString> downloadTargets;
    QMultiHash<QUrl, QnId> peerAssociations;
    QHash<QUrl, QString> hashByUrl;
    QHash<QUrl, qint64> fileSizeByUrl;

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        QList<QnId> peers = m_idBySystemInformation.values(it.key());
        if (peers.isEmpty())
            continue;

        QString fileName = it.value()->fileName;
        if (fileName.isEmpty())
            fileName = updateFilePath(updatesDirName, it.value()->baseFileName);

        if (!fileName.isEmpty() && verifyFile(fileName, it.value()->fileSize, it.value()->md5)) {
            it.value()->fileName = fileName;
            continue;
        }

        downloadTargets.insert(it.value()->url, it.value()->baseFileName);
        hashByUrl.insert(it.value()->url, it.value()->md5);
        fileSizeByUrl.insert(it.value()->url, it.value()->fileSize);
        foreach (const QnId &peerId, peers) {
            peerAssociations.insert(it.value()->url, peerId);
            PeerUpdateInformation &updateInformation = m_updateInformationById[peerId];
            updateInformation.state = PeerUpdateInformation::UpdateDownloading;
            updateInformation.progress = 0;
        }
    }

    if (!m_clientRequiresInstaller) {
        QString fileName = m_clientUpdateFile->fileName;
        if (fileName.isEmpty())
            fileName = updateFilePath(updatesDirName, m_clientUpdateFile->baseFileName);

        if (!fileName.isEmpty() && verifyFile(fileName, m_clientUpdateFile->fileSize, m_clientUpdateFile->md5)) {
            m_clientUpdateFile->fileName = fileName;
        } else {
            downloadTargets.insert(m_clientUpdateFile->url, m_clientUpdateFile->baseFileName);
            hashByUrl.insert(m_clientUpdateFile->url, m_clientUpdateFile->md5);
            fileSizeByUrl.insert(m_clientUpdateFile->url, m_clientUpdateFile->fileSize);
            peerAssociations.insert(m_clientUpdateFile->url, qnCommon->moduleGUID());
        }
    }

    emit progressChanged(0);
    setState(DownloadingUpdate);

    m_downloadUpdatesPeerTask->setTargetDir(updatesDirName);
    m_downloadUpdatesPeerTask->setTargets(downloadTargets);
    m_downloadUpdatesPeerTask->setHashes(hashByUrl);
    m_downloadUpdatesPeerTask->setFileSizes(fileSizeByUrl);
    m_downloadUpdatesPeerTask->setPeerAssociations(peerAssociations);
    m_downloadUpdatesPeerTask->setPeers(QSet<QnId>::fromList(m_updateInformationById.keys()));
    QMetaObject::invokeMethod(m_downloadUpdatesPeerTask, "start", Qt::QueuedConnection);
}

void QnMediaServerUpdateTool::uploadUpdatesToServers() {
    QHash<QnSystemInformation, QString> fileBySystemInformation;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it)
        fileBySystemInformation[it.key()] = it.value()->fileName;

    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it) {
        if (!m_incompatiblePeerIds.contains(it.key()))
            it->state = PeerUpdateInformation::UpdateUploading;
    }

    emit progressChanged(0);
    setState(UploadingUpdate);

    m_uploadUpdatesPeerTask->setUpdateId(m_updateId);
    m_uploadUpdatesPeerTask->setUploads(fileBySystemInformation);
    m_uploadUpdatesPeerTask->start(m_targetPeerIds - m_incompatiblePeerIds);
}

void QnMediaServerUpdateTool::installClientUpdate() {
    if (m_clientRequiresInstaller || m_disableClientUpdates || m_clientUpdateFile->version == qnCommon->engineVersion()) {
        installIncompatiblePeers();
        return;
    }

    setState(InstallingClientUpdate);

    QFuture<applauncher::api::ResultType::Value> future = QtConcurrent::run(&applauncher::installZip, m_targetVersion, m_clientUpdateFile->fileName);
    QFutureWatcher<applauncher::api::ResultType::Value> *futureWatcher = new QFutureWatcher<applauncher::api::ResultType::Value>(this);
    futureWatcher->setFuture(future);
    connect(futureWatcher, &QFutureWatcher<applauncher::api::ResultType::Value>::finished, this, &QnMediaServerUpdateTool::at_clientUpdateInstalled);
}

void QnMediaServerUpdateTool::installUpdatesToServers() {
    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it) {
        if (!m_incompatiblePeerIds.contains(it.key()))
            it->state = PeerUpdateInformation::UpdateInstalling;
    }

    setState(InstallingUpdate);

    m_installUpdatesPeerTask->setUpdateId(m_updateId);
    m_installUpdatesPeerTask->setVersion(m_targetVersion);
    m_installUpdatesPeerTask->setPeers(m_targetPeerIds - m_incompatiblePeerIds);
    QMetaObject::invokeMethod(m_installUpdatesPeerTask, "start", Qt::QueuedConnection);
}

void QnMediaServerUpdateTool::installIncompatiblePeers() {
    QHash<QnSystemInformation, QString> updateFiles;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        updateFiles[it.key()] = it.value()->fileName;

        foreach (const QnId &peerId, m_idBySystemInformation.values(it.key())) {
            if (m_incompatiblePeerIds.contains(peerId))
                m_updateInformationById[peerId].state = PeerUpdateInformation::UpdateInstalling;
        }
    }

    setState(InstallingToIncompatiblePeers);

    m_restUpdatePeerTask->setUpdateId(m_updateId);
    m_restUpdatePeerTask->setUpdateFiles(updateFiles);
    m_restUpdatePeerTask->setVersion(m_targetVersion);
    m_restUpdatePeerTask->setPeers(m_incompatiblePeerIds);
    QMetaObject::invokeMethod(m_restUpdatePeerTask, "start", Qt::QueuedConnection);
}

void QnMediaServerUpdateTool::lockMutex() {
    m_distributedMutex = ec2::QnDistributedMutexManager::instance()->createMutex(mutexName);
    connect(m_distributedMutex, &ec2::QnDistributedMutex::locked,        this,   &QnMediaServerUpdateTool::at_mutexLocked, Qt::QueuedConnection);
    connect(m_distributedMutex, &ec2::QnDistributedMutex::lockTimeout,   this,   &QnMediaServerUpdateTool::at_mutexTimeout, Qt::QueuedConnection);
    m_distributedMutex->lockAsync();
}

void QnMediaServerUpdateTool::unlockMutex() {
    if (m_distributedMutex) {
        m_distributedMutex->unlock();
        m_distributedMutex->deleteLater();
        m_distributedMutex = 0;
    }
}

void QnMediaServerUpdateTool::at_mutexLocked() {
    uploadUpdatesToServers();
}

void QnMediaServerUpdateTool::at_mutexTimeout() {
    m_distributedMutex->deleteLater();
    m_distributedMutex = 0;
    finishUpdate(LockFailed);
}

void QnMediaServerUpdateTool::at_checkForUpdatesTask_finished(int errorCode) {
    m_tasksThread->quit();

    m_updateFiles = m_checkForUpdatesPeerTask->updateFiles();
    m_localTemporaryDir = m_checkForUpdatesPeerTask->temporaryDir();
    m_targetVersion = m_checkForUpdatesPeerTask->targetVersion();
    m_clientRequiresInstaller = m_checkForUpdatesPeerTask->isClientRequiresInstaller();
    m_clientUpdateFile = m_checkForUpdatesPeerTask->clientUpdateFile();

    setCheckResult((CheckResult)errorCode); // codes are the same now
}

void QnMediaServerUpdateTool::at_downloadTask_finished(int errorCode) {
    if (m_state != DownloadingUpdate)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(DownloadingFailed);
        return;
    }

    QHash<QUrl, QString> resultingFiles = m_downloadUpdatesPeerTask->resultingFiles();

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        if (resultingFiles.contains(it.value()->url))
            it.value()->fileName = resultingFiles[it.value()->url];
    }
    if (resultingFiles.contains(m_clientUpdateFile->url))
        m_clientUpdateFile->fileName = resultingFiles[m_clientUpdateFile->url];

    installClientUpdate();
}

void QnMediaServerUpdateTool::at_clientUpdateInstalled() {
    QFutureWatcher<applauncher::api::ResultType::Value> *futureWatcher = dynamic_cast<QFutureWatcher<applauncher::api::ResultType::Value>*>(sender());
    if (!futureWatcher)
        return;

    futureWatcher->deleteLater();

    if (futureWatcher->result() != applauncher::api::ResultType::ok) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(InstallationFailed);
        return;
    }

    installIncompatiblePeers();
}

void QnMediaServerUpdateTool::at_uploadTask_finished(int errorCode) {
    if (m_state != UploadingUpdate)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(UploadingFailed);
        return;
    }

    installUpdatesToServers();
}

void QnMediaServerUpdateTool::at_installTask_finished(int errorCode) {
    if (m_state != InstallingUpdate)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(InstallationFailed);
        return;
    }

    finishUpdate(UpdateSuccessful);
}

void QnMediaServerUpdateTool::at_restUpdateTask_finished(int errorCode) {
    if (m_state != InstallingToIncompatiblePeers)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(RestInstallationFailed);
        return;
    }

    if ((m_targetPeerIds - m_incompatiblePeerIds).isEmpty()) {
        finishUpdate(UpdateSuccessful);
        return;
    }

    lockMutex();
}

void QnMediaServerUpdateTool::at_networkTask_peerProgressChanged(const QnId &peerId, int progress) {
    m_updateInformationById[peerId].progress = progress;
    emit peerChanged(peerId);
}

void QnMediaServerUpdateTool::at_downloadTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::PendingUpload);
}

void QnMediaServerUpdateTool::at_uploadTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::PendingInstallation);
}

void QnMediaServerUpdateTool::at_installTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::UpdateFinished);
}

void QnMediaServerUpdateTool::at_restUpdateTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::UpdateFinished);
}
