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
#include <mutex/distributed_mutex_manager.h>
#include <client/client_settings.h>

#include <version.h>

namespace {

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
    m_distributedMutex(0),
    m_checkForUpdatesPeerTask(new QnCheckForUpdatesPeerTask()),
    m_downloadUpdatesPeerTask(new QnDownloadUpdatesPeerTask()),
    m_uploadUpdatesPeerTask(new QnUploadUpdatesPeerTask()),
    m_installUpdatesPeerTask(new QnInstallUpdatesPeerTask()),
    m_restUpdatePeerTask(new QnRestUpdatePeerTask()),
    m_clientRequiresInstaller(false),
    m_disableClientUpdates(defaultDisableClientUpdates)
{
    m_checkForUpdatesPeerTask->setUpdatesUrl(QUrl(qnSettings->updateFeedUrl()));

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
    connect(m_restUpdatePeerTask,                       &QnRestUpdatePeerTask::peerUpdateFinished,      this,   &QnMediaServerUpdateTool::at_restUpdateTask_peerUpdateFinished);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::progressChanged,            this,   &QnMediaServerUpdateTool::at_taskProgressChanged);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::progressChanged,            this,   &QnMediaServerUpdateTool::at_taskProgressChanged);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::peerProgressChanged,        this,   &QnMediaServerUpdateTool::at_networkTask_peerProgressChanged);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::peerProgressChanged,        this,   &QnMediaServerUpdateTool::at_networkTask_peerProgressChanged);


    auto targetsWatcher = [this] {
        if (!m_targets.isEmpty())
            return;
        emit targetsChanged(actualTargetIds());
    };

    connect(qnResPool,  &QnResourcePool::resourceAdded, this, targetsWatcher);
    connect(qnResPool,  &QnResourcePool::resourceRemoved, this, targetsWatcher);
}

QnMediaServerUpdateTool::~QnMediaServerUpdateTool() {
    if (m_tasksThread->isRunning()) {
        m_tasksThread->quit();
        m_tasksThread->wait();
    }

    delete m_checkForUpdatesPeerTask;
    delete m_downloadUpdatesPeerTask;
    delete m_uploadUpdatesPeerTask;
    delete m_installUpdatesPeerTask;
    delete m_restUpdatePeerTask;
}

QnMediaServerUpdateTool::State QnMediaServerUpdateTool::state() const {
    return m_state;
}

bool QnMediaServerUpdateTool::isUpdating() const {
    return m_state >= DownloadingUpdate;
}

bool QnMediaServerUpdateTool::idle() const {
    return m_state == Idle;
}

void QnMediaServerUpdateTool::setState(State state) {
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);

    QnFullUpdateStage stage = QnFullUpdateStage::Download;
    int offset = 0; // infinite stages

    switch (m_state) {
    case DownloadingUpdate:
        break;
    case InstallingClientUpdate:
        stage = QnFullUpdateStage::Client;
        offset = 50;
        break;
    case InstallingToIncompatiblePeers:
        stage = QnFullUpdateStage::Incompatible;
        offset = 50;
        break;
    case UploadingUpdate:
        stage = QnFullUpdateStage::Push;
        break;
    case InstallingUpdate:
        stage = QnFullUpdateStage::Servers;
        offset = 50;
        break;
    default:
        return;
    }

    int progress = (static_cast<int>(stage)*100 + offset) / ( static_cast<int>(QnFullUpdateStage::Count) );
    emit progressChanged(progress);    
}

void QnMediaServerUpdateTool::finishUpdate(QnUpdateResult result) {
    m_tasksThread->quit();
    unlockMutex();
    removeTemporaryDir();
    setState(Idle);
    emit updateFinished(result);
}

void QnMediaServerUpdateTool::setPeerState(const QUuid &peerId, QnMediaServerUpdateTool::PeerUpdateInformation::State state) {
    auto it = m_updateInformationById.find(peerId);
    if (it == m_updateInformationById.end())
        return;

    if (it->state != state) {
        it->state = state;
        emit peerChanged(peerId);
    }
}

QnSoftwareVersion QnMediaServerUpdateTool::targetVersion() const {
    return m_targetVersion;
}

QnMediaServerUpdateTool::PeerUpdateInformation QnMediaServerUpdateTool::updateInformation(const QUuid &peerId) const {
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

void QnMediaServerUpdateTool::setTargets(const QSet<QUuid> &targets, bool client) {
    m_targets.clear();

    QSet<QUuid> suitableTargets;

    foreach (const QUuid &id, targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;
        m_targets.append(server);
        suitableTargets.insert(id);
    }

    m_disableClientUpdates = !client;

    emit targetsChanged(suitableTargets);
}

QnMediaServerResourceList QnMediaServerUpdateTool::actualTargets() const {
    if (!m_targets.isEmpty())
        return m_targets;

    QnMediaServerResourceList result = qnResPool->getResourcesWithFlag(Qn::server).filtered<QnMediaServerResource>();

    foreach (const QnMediaServerResourcePtr &server, qnResPool->getAllIncompatibleResources().filtered<QnMediaServerResource>()) {
        if (server->getSystemName() == qnCommon->localSystemName())
            result.append(server);
    }
    return result;
}

QSet<QUuid> QnMediaServerUpdateTool::actualTargetIds() const {
    QSet<QUuid> targets;        
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        targets.insert(server->getId());
    return targets;
}

QUrl QnMediaServerUpdateTool::generateUpdatePackageUrl(const QnSoftwareVersion &targetVersion) const {
    QUrlQuery query;

    QString versionSuffix;
    if (targetVersion.isNull()) {
        versionSuffix = lit("/latest");
        query.addQueryItem(lit("current"), qnCommon->engineVersion().toString());
    } else {
        versionSuffix = QString(lit("/%1-%2"))
            .arg(targetVersion.toString(), passwordForBuild(static_cast<unsigned>(targetVersion.build())));
    }

    QSet<QnSystemInformation> systemInformationList;
    foreach (const QnMediaServerResourcePtr &server, actualTargets()) {
        bool incompatible = (server->getStatus() == Qn::Incompatible);

        if (server->getStatus() != Qn::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        if (!targetVersion.isNull() && server->getVersion() == targetVersion)
            continue;

        systemInformationList.insert(server->getSystemInfo());
    }

    query.addQueryItem(lit("client"), QnSystemInformation::currentSystemInformation().toString().replace(L' ', L'_'));
    foreach (const QnSystemInformation &systemInformation, systemInformationList)
        query.addQueryItem(lit("server"), systemInformation.toString().replace(L' ', L'_'));

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

void QnMediaServerUpdateTool::checkForUpdates(const QnSoftwareVersion &version, bool denyMajorUpdates) {
    if (m_state >= CheckingForUpdates)
        return;

    setState(CheckingForUpdates);

    QSet<QUuid> peers;
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        peers.insert(server->getId());

    m_checkForUpdatesPeerTask->setPeers(peers);
    m_checkForUpdatesPeerTask->setTargetVersion(version);
    m_checkForUpdatesPeerTask->setDisableClientUpdates(m_disableClientUpdates || qnSettings->isClientUpdateDisabled());
    m_checkForUpdatesPeerTask->setDenyMajorUpdates(denyMajorUpdates);
    m_tasksThread->start();
    QMetaObject::invokeMethod(m_checkForUpdatesPeerTask, "start", Qt::QueuedConnection);
}

void QnMediaServerUpdateTool::checkForUpdates(const QString &fileName) {
    if (m_state >= CheckingForUpdates)
        return;

    setState(CheckingForUpdates);

    QSet<QUuid> peers;
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        peers.insert(server->getId());

    m_checkForUpdatesPeerTask->setPeers(peers);
    m_checkForUpdatesPeerTask->setUpdateFileName(fileName);
    m_checkForUpdatesPeerTask->setDisableClientUpdates(m_disableClientUpdates || qnSettings->isClientUpdateDisabled());
    m_checkForUpdatesPeerTask->setDenyMajorUpdates(false);
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
    if (m_state != Idle)
        return;

    m_updateId = QUuid::createUuid().toString();
    m_updateInformationById.clear();
    m_idBySystemInformation.clear();

    foreach (const QnMediaServerResourcePtr &server, actualTargets()) {
        bool incompatible = (server->getStatus() == Qn::Incompatible);

        if (server->getStatus() != Qn::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        if (!m_targetVersion.isNull() && server->getVersion() == m_targetVersion)
            continue;

        QUuid peerId = server->getId();

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
        return false;   /* Cannot cancel update at this state. */
    default:
        break;
    }

    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
        it->state = PeerUpdateInformation::UpdateCanceled;

    finishUpdate(QnUpdateResult::Cancelled);
    return true;
}

void QnMediaServerUpdateTool::downloadUpdates() {
    QHash<QUrl, QString> downloadTargets;
    QMultiHash<QUrl, QUuid> peerAssociations;
    QHash<QUrl, QString> hashByUrl;
    QHash<QUrl, qint64> fileSizeByUrl;

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        QList<QUuid> peers = m_idBySystemInformation.values(it.key());
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
        foreach (const QUuid &peerId, peers) {
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

    setState(DownloadingUpdate);

    m_downloadUpdatesPeerTask->setTargetDir(updatesDirName);
    m_downloadUpdatesPeerTask->setTargets(downloadTargets);
    m_downloadUpdatesPeerTask->setHashes(hashByUrl);
    m_downloadUpdatesPeerTask->setFileSizes(fileSizeByUrl);
    m_downloadUpdatesPeerTask->setPeerAssociations(peerAssociations);
    m_downloadUpdatesPeerTask->setPeers(QSet<QUuid>::fromList(m_updateInformationById.keys()));
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

    m_uploadUpdatesPeerTask->setUpdateId(m_updateId);
    m_uploadUpdatesPeerTask->setUploads(fileBySystemInformation);
    m_uploadUpdatesPeerTask->start(m_targetPeerIds - m_incompatiblePeerIds);
}

void QnMediaServerUpdateTool::installClientUpdate() {
    if (m_clientRequiresInstaller || m_disableClientUpdates || qnSettings->isClientUpdateDisabled() || m_clientUpdateFile->version == qnCommon->engineVersion()) {
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
    setState(InstallingUpdate);

    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it) {
        if (!m_incompatiblePeerIds.contains(it.key()))
            it->state = PeerUpdateInformation::UpdateInstalling;
    }

    m_installUpdatesPeerTask->setUpdateId(m_updateId);
    m_installUpdatesPeerTask->setVersion(m_targetVersion);
    m_installUpdatesPeerTask->setPeers(m_targetPeerIds - m_incompatiblePeerIds);
    QMetaObject::invokeMethod(m_installUpdatesPeerTask, "start", Qt::QueuedConnection);
}

void QnMediaServerUpdateTool::installIncompatiblePeers() {
    QHash<QnSystemInformation, QString> updateFiles;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        updateFiles[it.key()] = it.value()->fileName;

        foreach (const QUuid &peerId, m_idBySystemInformation.values(it.key())) {
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

void QnMediaServerUpdateTool::prepareToUpload() {
    connect(qnResPool, &QnResourcePool::statusChanged, this, &QnMediaServerUpdateTool::at_resourcePool_statusChanged);

    foreach (const QnMediaServerResourcePtr &server, m_targets) {
        if (server->getStatus() != Qn::Online) {
            finishUpdate(QnUpdateResult::UploadingFailed);
            return;
        }
    }

    lockMutex();
}

void QnMediaServerUpdateTool::lockMutex() {
    setState(UploadingUpdate);

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
    finishUpdate(QnUpdateResult::LockFailed);
}

void QnMediaServerUpdateTool::at_checkForUpdatesTask_finished(int errorCode) {
    m_tasksThread->quit();

    m_updateFiles = m_checkForUpdatesPeerTask->updateFiles();
    m_localTemporaryDir = m_checkForUpdatesPeerTask->temporaryDir();
    m_targetVersion = m_checkForUpdatesPeerTask->targetVersion();
    m_clientRequiresInstaller = m_checkForUpdatesPeerTask->isClientRequiresInstaller();
    m_clientUpdateFile = m_checkForUpdatesPeerTask->clientUpdateFile();

    setState(Idle);
    emit checkForUpdatesFinished(static_cast<QnCheckForUpdateResult>(errorCode));
}

void QnMediaServerUpdateTool::at_downloadTask_finished(int errorCode) {
    if (m_state != DownloadingUpdate)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::DownloadingFailed);
        return;
    }

    QHash<QUrl, QString> resultingFiles = m_downloadUpdatesPeerTask->resultingFiles();

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        if (resultingFiles.contains(it.value()->url))
            it.value()->fileName = resultingFiles[it.value()->url];
    }
    if (!m_clientRequiresInstaller && resultingFiles.contains(m_clientUpdateFile->url))
        m_clientUpdateFile->fileName = resultingFiles[m_clientUpdateFile->url];

    installClientUpdate();
}

void QnMediaServerUpdateTool::at_clientUpdateInstalled() {
    QFutureWatcher<applauncher::api::ResultType::Value> *futureWatcher = dynamic_cast<QFutureWatcher<applauncher::api::ResultType::Value>*>(sender());
    if (!futureWatcher)
        return;

    futureWatcher->deleteLater();

    if (m_state != InstallingClientUpdate)
        return;

    if (futureWatcher->result() != applauncher::api::ResultType::ok) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::InstallationFailed);
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
        finishUpdate(QnUpdateResult::UploadingFailed);
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
        finishUpdate(QnUpdateResult::InstallationFailed);
        return;
    }

    finishUpdate(QnUpdateResult::Successful);
}

void QnMediaServerUpdateTool::at_restUpdateTask_finished(int errorCode) {
    if (m_state != InstallingToIncompatiblePeers)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::RestInstallationFailed);
        return;
    }

    if ((m_targetPeerIds - m_incompatiblePeerIds).isEmpty()) {
        finishUpdate(QnUpdateResult::Successful);
        return;
    }

    prepareToUpload();
}

void QnMediaServerUpdateTool::at_taskProgressChanged(int progress) {
    QnFullUpdateStage stage = QnFullUpdateStage::Download;
    switch (m_state) {
    case DownloadingUpdate:
        break;
    case UploadingUpdate:
        stage = QnFullUpdateStage::Push;
        break;
    default:
        return;
    }

    int fullProgress = (progress + static_cast<int>(stage)*100) / ( static_cast<int>(QnFullUpdateStage::Count) ) ;
    emit progressChanged(fullProgress);
}


void QnMediaServerUpdateTool::at_networkTask_peerProgressChanged(const QUuid &peerId, int progress) {

    QnPeerUpdateStage stage = QnPeerUpdateStage::Download;

    switch (m_updateInformationById[peerId].state) {
    case PeerUpdateInformation::PendingUpload:
    case PeerUpdateInformation::UpdateUploading:
        stage = QnPeerUpdateStage::Push;
        break;
    case PeerUpdateInformation::PendingInstallation:
    case PeerUpdateInformation::UpdateInstalling:
        stage = QnPeerUpdateStage::Install;
        progress = 50;
        break;
    default:
        break;
    }

    m_updateInformationById[peerId].progress = (progress + static_cast<int>(stage)*100) / ( static_cast<int>(QnPeerUpdateStage::Count) ) ;
    emit peerChanged(peerId);
}

void QnMediaServerUpdateTool::at_resourcePool_statusChanged(const QnResourcePtr &resource) {
    if (m_state != UploadingUpdate)
        return;

    if (!m_targetPeerIds.contains(resource->getId()))
        return;

    if (resource->getStatus() != Qn::Online) {
        m_uploadUpdatesPeerTask->cancel();
        finishUpdate(QnUpdateResult::UploadingFailed);
    }
}

void QnMediaServerUpdateTool::at_downloadTask_peerFinished(const QUuid &peerId) {
    setPeerState(peerId, PeerUpdateInformation::PendingUpload);
}

void QnMediaServerUpdateTool::at_uploadTask_peerFinished(const QUuid &peerId) {
    setPeerState(peerId, PeerUpdateInformation::PendingInstallation);
}

void QnMediaServerUpdateTool::at_installTask_peerFinished(const QUuid &peerId) {
    setPeerState(peerId, PeerUpdateInformation::UpdateFinished);
}

void QnMediaServerUpdateTool::at_restUpdateTask_peerUpdateFinished(const QUuid &incompatibleId, const QUuid &id) {
    PeerUpdateInformation info = m_updateInformationById.take(incompatibleId);
    info.state = PeerUpdateInformation::UpdateFinished;
    info.server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
    m_updateInformationById.insert(id, info);
    emit targetsChanged(QSet<QUuid>::fromList(m_updateInformationById.keys()));
    emit peerChanged(id);
}
