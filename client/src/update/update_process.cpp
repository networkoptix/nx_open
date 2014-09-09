#include "update_process.h"

#include <QtCore/QFutureWatcher>
#include <QtConcurrent>

#include <client/client_settings.h>
#include <common/common_module.h>
#include <common/common_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <mutex/distributed_mutex_manager.h>
#include <mutex/distributed_mutex.h>

#include <update/task/check_update_peer_task.h>
#include <update/task/download_updates_peer_task.h>
#include <update/task/rest_update_peer_task.h>
#include <update/task/upload_updates_peer_task.h>
#include <update/task/install_updates_peer_task.h>

#include <utils/applauncher_utils.h>
#include <utils/update/update_utils.h>

#include <version.h>

namespace {

    const QString updatesDirName = lit(QN_PRODUCT_NAME_SHORT) + lit("_updates");
    const QString mutexName = lit("auto_update");

    bool verifyFile(const QString &fileName, qint64 size, const QString &md5) {
        QFile file(fileName);

        if (!file.exists() || file.size() != size)
            return false;

        if (!md5.isEmpty() && makeMd5(&file) != md5)
            return false;

        return true;
    }

}

QnPeerUpdateInformation::QnPeerUpdateInformation(const QnMediaServerResourcePtr &server /*= QnMediaServerResourcePtr()*/):
    server(server),
    state(UpdateUnknown),
    updateInformation(0),
    progress(0)
{
    if (server)
        sourceVersion = server->getVersion();
}

QnUpdateProcess::QnUpdateProcess(const QnUpdateTarget &target, QObject *parent):
    base_type(parent),
    m_id(QUuid::createUuid()),
    m_target(target),
    m_stage(QnFullUpdateStage::Init),
    m_distributedMutex(NULL),
    m_clientRequiresInstaller(true)
{
   
}

void QnUpdateProcess::start() {
    QnCheckForUpdatesPeerTask *checkForUpdatesTask = new QnCheckForUpdatesPeerTask(m_target);
    connect(checkForUpdatesTask,  &QnCheckForUpdatesPeerTask::checkFinished,   this,  [this, checkForUpdatesTask](const QnCheckForUpdateResult &result){
        at_checkForUpdatesTaskFinished(checkForUpdatesTask, result);
    });
    connect(checkForUpdatesTask,  &QnNetworkPeerTask::finished,  checkForUpdatesTask, &QObject::deleteLater);
    m_currentTask = checkForUpdatesTask;
    setStage(QnFullUpdateStage::Check);
    checkForUpdatesTask->start();
}

void QnUpdateProcess::downloadUpdates() {

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
            QnPeerUpdateInformation &updateInformation = m_updateInformationById[peerId];
            updateInformation.state = QnPeerUpdateInformation::UpdateDownloading;
            updateInformation.progress = 0;
            emit peerStageChanged(peerId, QnPeerUpdateStage::Download);
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


    QnDownloadUpdatesPeerTask *downloadUpdatesPeerTask = new QnDownloadUpdatesPeerTask();
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             this,  [this, downloadUpdatesPeerTask](int errorCode) {
        at_downloadTaskFinished(downloadUpdatesPeerTask, errorCode);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::peerFinished,         this,  [this](const QUuid &peerId) {
        setPeerState(peerId, QnPeerUpdateInformation::PendingUpload);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::progressChanged,      this,  &QnUpdateProcess::progressChanged);
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::peerProgressChanged,  this,  [this](const QUuid &peerId, int progress) {
        emit peerStageProgressChanged(peerId, QnPeerUpdateStage::Download, progress);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             downloadUpdatesPeerTask, &QObject::deleteLater);

    downloadUpdatesPeerTask->setTargetDir(updatesDirName);
    downloadUpdatesPeerTask->setTargets(downloadTargets);
    downloadUpdatesPeerTask->setHashes(hashByUrl);
    downloadUpdatesPeerTask->setFileSizes(fileSizeByUrl);
    downloadUpdatesPeerTask->setPeerAssociations(peerAssociations);
    m_currentTask = downloadUpdatesPeerTask;
    setStage(QnFullUpdateStage::Download);
    downloadUpdatesPeerTask->start(QSet<QUuid>::fromList(m_updateInformationById.keys()));
}


void QnUpdateProcess::setPeerState(const QUuid &peerId, QnPeerUpdateInformation::State state) {
    if (!m_updateInformationById.contains(peerId))
        return;

    QnPeerUpdateInformation &info = m_updateInformationById[peerId];
    if (info.state == state) 
        return;
    
    auto stateToStage = [](QnPeerUpdateInformation::State state) {
        switch (state) {
        case QnPeerUpdateInformation::PendingDownloading:
        case QnPeerUpdateInformation::UpdateDownloading:
            return QnPeerUpdateStage::Download;
        case QnPeerUpdateInformation::PendingUpload:
        case QnPeerUpdateInformation::UpdateUploading:
            return QnPeerUpdateStage::Push;
        case QnPeerUpdateInformation::PendingInstallation:
        case QnPeerUpdateInformation::UpdateInstalling:
            return QnPeerUpdateStage::Install;
        default:
            return QnPeerUpdateStage::Init;
        }
    };

    QnPeerUpdateStage oldStage = stateToStage(info.state);
    info.state = state;
    QnPeerUpdateStage stage = stateToStage(state);
    if (oldStage != stage)
        emit peerStageChanged(peerId, stage);
}

void QnUpdateProcess::setAllPeersState(QnPeerUpdateInformation::State state) {
    foreach(const QUuid &key, m_updateInformationById.keys())
        setPeerState(key, state);
}


void QnUpdateProcess::setCompatiblePeersState(QnPeerUpdateInformation::State state) {
    foreach(const QUuid &key, m_updateInformationById.keys())
        if (!m_incompatiblePeerIds.contains(key))
            setPeerState(key, state);
}

void QnUpdateProcess::setIncompatiblePeersState(QnPeerUpdateInformation::State state) {
    foreach(const QUuid &key, m_updateInformationById.keys())
        if (m_incompatiblePeerIds.contains(key))
            setPeerState(key, state);
}

void QnUpdateProcess::at_checkForUpdatesTaskFinished(QnCheckForUpdatesPeerTask* task, const QnCheckForUpdateResult &result) {
    if (result.result != QnCheckForUpdateResult::UpdateFound) {
        setAllPeersState(QnPeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::DownloadingFailed);
        return;
    }

    m_target.version = result.latestVersion; /* Version can be updated if loading from local file or seeking for latest version. */
    m_clientRequiresInstaller = result.clientInstallerRequired;
    m_localTemporaryDir = task->temporaryDir();
    m_updateFiles = task->updateFiles();
    m_clientUpdateFile = task->clientUpdateFile();

    foreach (const QUuid &serverId, m_target.targets) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(serverId).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        bool incompatible = (server->getStatus() == Qn::Incompatible);

        if (server->getStatus() != Qn::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        if (!m_target.version.isNull() && server->getVersion() == m_target.version)
            continue;

        QUuid peerId = server->getId();

        m_targetPeerIds.insert(peerId);
        if (incompatible)
            m_incompatiblePeerIds.insert(peerId);

        QnPeerUpdateInformation info(server);
        info.updateInformation = m_updateFiles.value(server->getSystemInfo());

        if (!info.updateInformation) {
            Q_ASSERT_X(0, "No update info for server", Q_FUNC_INFO);
            return;
        }

        info.state = info.updateInformation->fileName.isEmpty() ? QnPeerUpdateInformation::PendingDownloading
            : QnPeerUpdateInformation::PendingUpload;

        m_updateInformationById.insert(peerId, info);
        m_idBySystemInformation.insert(server->getSystemInfo(), peerId);
    }

    downloadUpdates();
}


void QnUpdateProcess::at_downloadTaskFinished(QnDownloadUpdatesPeerTask* task, int errorCode) {
    if (errorCode != 0) {
        setAllPeersState(QnPeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::DownloadingFailed);
        return;
    }

    QHash<QUrl, QString> resultingFiles = task->resultingFiles();

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        if (resultingFiles.contains(it.value()->url))
            it.value()->fileName = resultingFiles[it.value()->url];
    }
    if (!m_clientRequiresInstaller && resultingFiles.contains(m_clientUpdateFile->url))
        m_clientUpdateFile->fileName = resultingFiles[m_clientUpdateFile->url];

    installClientUpdate();
}

void QnUpdateProcess::finishUpdate(QnUpdateResult result) {
    unlockMutex();
    removeTemporaryDir();
    emit updateFinished(result);
}

void QnUpdateProcess::installClientUpdate() {
    /* Check if we skip this step. */
    if (m_clientRequiresInstaller 
        || m_target.denyClientUpdates
        || qnSettings->isClientUpdateDisabled() 
        || m_clientUpdateFile->version == qnCommon->engineVersion()) {
            installIncompatiblePeers();
            return;
    }

    setStage(QnFullUpdateStage::Client);

    QFuture<applauncher::api::ResultType::Value> future = QtConcurrent::run(&applauncher::installZip, m_target.version, m_clientUpdateFile->fileName);
    QFutureWatcher<applauncher::api::ResultType::Value> *futureWatcher = new QFutureWatcher<applauncher::api::ResultType::Value>(this);
    futureWatcher->setFuture(future);
    connect(futureWatcher, &QFutureWatcher<applauncher::api::ResultType::Value>::finished, this, &QnUpdateProcess::at_clientUpdateInstalled);
    connect(futureWatcher, &QFutureWatcher<applauncher::api::ResultType::Value>::finished, futureWatcher, &QObject::deleteLater);
}

void QnUpdateProcess::setStage(QnFullUpdateStage stage) {
    if (m_stage == stage)
        return;
    m_stage = stage;
    emit stageChanged(stage);
}

void QnUpdateProcess::at_clientUpdateInstalled() {
    QFutureWatcher<applauncher::api::ResultType::Value> *futureWatcher = dynamic_cast<QFutureWatcher<applauncher::api::ResultType::Value>*>(sender());
    if (!futureWatcher)
        return;

    if (futureWatcher->result() != applauncher::api::ResultType::ok) {
        setAllPeersState(QnPeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::ClientInstallationFailed);
        return;
    }

    installIncompatiblePeers();
}

void QnUpdateProcess::installIncompatiblePeers() {
    QHash<QnSystemInformation, QString> targetFiles;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        targetFiles[it.key()] = it.value()->fileName;
        setIncompatiblePeersState(QnPeerUpdateInformation::UpdateInstalling);
    }

    QnRestUpdatePeerTask* restUpdatePeerTask = new QnRestUpdatePeerTask();
    restUpdatePeerTask->setUpdateId(m_id.toString());
    restUpdatePeerTask->setUpdateFiles(targetFiles);
    restUpdatePeerTask->setVersion(m_target.version);
    connect(restUpdatePeerTask, &QnNetworkPeerTask::finished,                   this,   &QnUpdateProcess::at_restUpdateTask_finished);
    connect(restUpdatePeerTask, &QnRestUpdatePeerTask::peerUpdateFinished,      this,   &QnUpdateProcess::at_restUpdateTask_peerUpdateFinished);
    connect(restUpdatePeerTask, &QnNetworkPeerTask::finished,                   restUpdatePeerTask,   &QObject::deleteLater);
    m_currentTask = restUpdatePeerTask;
    setStage(QnFullUpdateStage::Incompatible);
    restUpdatePeerTask->start(m_incompatiblePeerIds);
}

void QnUpdateProcess::at_restUpdateTask_peerUpdateFinished(const QUuid &incompatibleId, const QUuid &id) {
    QnPeerUpdateInformation info = m_updateInformationById.take(incompatibleId);
    info.state = QnPeerUpdateInformation::UpdateFinished;
    info.server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
    m_updateInformationById.insert(id, info);
    emit targetsChanged(QSet<QUuid>::fromList(m_updateInformationById.keys()));
    emit peerStageChanged(id, QnPeerUpdateStage::Init);
}

void QnUpdateProcess::at_restUpdateTask_finished(int errorCode) {
    if (errorCode != 0) {
        setAllPeersState(QnPeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::RestInstallationFailed);
        return;
    }

    if ((m_targetPeerIds - m_incompatiblePeerIds).isEmpty()) {
        finishUpdate(QnUpdateResult::Successful);
        return;
    }

    prepareToUpload();
}

void QnUpdateProcess::prepareToUpload() {
    foreach (const QUuid &target, m_targetPeerIds) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(target).dynamicCast<QnMediaServerResource>();
        if (!server || server->getStatus() != Qn::Online) {
            finishUpdate(QnUpdateResult::UploadingFailed);
            return;
        }
    }
    lockMutex();
}

void QnUpdateProcess::lockMutex() {
    setStage(QnFullUpdateStage::Push);

    m_distributedMutex = ec2::QnDistributedMutexManager::instance()->createMutex(mutexName);
    connect(m_distributedMutex, &ec2::QnDistributedMutex::locked,        this,   &QnUpdateProcess::at_mutexLocked, Qt::QueuedConnection);
    connect(m_distributedMutex, &ec2::QnDistributedMutex::lockTimeout,   this,   &QnUpdateProcess::at_mutexTimeout, Qt::QueuedConnection);
    m_distributedMutex->lockAsync();
}

void QnUpdateProcess::unlockMutex() {
    if (m_distributedMutex) {
        m_distributedMutex->unlock();
        m_distributedMutex->deleteLater();
        m_distributedMutex = 0;
    }
}

void QnUpdateProcess::at_mutexLocked() {
    uploadUpdatesToServers();
}

void QnUpdateProcess::at_mutexTimeout() {
    m_distributedMutex->deleteLater();
    m_distributedMutex = 0;
    finishUpdate(QnUpdateResult::LockFailed);
}

void QnUpdateProcess::uploadUpdatesToServers() {
    QHash<QnSystemInformation, QString> fileBySystemInformation;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it)
        fileBySystemInformation[it.key()] = it.value()->fileName;

    setCompatiblePeersState(QnPeerUpdateInformation::UpdateUploading);

    QnUploadUpdatesPeerTask* uploadUpdatesPeerTask = new QnUploadUpdatesPeerTask();
    uploadUpdatesPeerTask->setUpdateId(m_id.toString());
    uploadUpdatesPeerTask->setUploads(fileBySystemInformation);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             this,     &QnUpdateProcess::at_uploadTask_finished);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::peerFinished,         this,     [this](const QUuid &peerId) {
            setPeerState(peerId, QnPeerUpdateInformation::PendingInstallation);
    });
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::progressChanged,      this,     &QnUpdateProcess::progressChanged);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::peerProgressChanged,  this,     [this](const QUuid &peerId, int progress) {
        emit peerStageProgressChanged(peerId, QnPeerUpdateStage::Push, progress);
    });
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             uploadUpdatesPeerTask,     &QObject::deleteLater);
    m_currentTask = uploadUpdatesPeerTask;
    uploadUpdatesPeerTask->start(m_targetPeerIds - m_incompatiblePeerIds);

    QPointer<QnUploadUpdatesPeerTask> uploadUpdatesPeerTaskPtr(uploadUpdatesPeerTask);

    connect(qnResPool, &QnResourcePool::statusChanged, this, [this, uploadUpdatesPeerTaskPtr](const QnResourcePtr &resource) {
        if (m_stage != QnFullUpdateStage::Push)
            return;

        if (!m_targetPeerIds.contains(resource->getId()))
            return;

        if (uploadUpdatesPeerTaskPtr.isNull())
            return;

        if (resource->getStatus() != Qn::Online) {
            uploadUpdatesPeerTaskPtr->cancel();
            finishUpdate(QnUpdateResult::UploadingFailed);
        }
    });
}

void QnUpdateProcess::installUpdatesToServers() {
    QnInstallUpdatesPeerTask* installUpdatesPeerTask = new QnInstallUpdatesPeerTask();
    installUpdatesPeerTask->setUpdateId(m_id.toString());
    installUpdatesPeerTask->setVersion(m_target.version);
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::finished,                   this,   &QnUpdateProcess::at_installTask_finished);
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::peerFinished,               this,   [this](const QUuid &peerId) {
        setPeerState(peerId, QnPeerUpdateInformation::UpdateFinished);
    });
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::finished,                   installUpdatesPeerTask,   &QObject::deleteLater);
    setStage(QnFullUpdateStage::Servers);
    setCompatiblePeersState(QnPeerUpdateInformation::UpdateInstalling);
    installUpdatesPeerTask->start(m_targetPeerIds - m_incompatiblePeerIds);
}

void QnUpdateProcess::at_uploadTask_finished(int errorCode) {
    if (errorCode != 0) {
        setAllPeersState(QnPeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::UploadingFailed);
        return;
    }

    installUpdatesToServers();
}


void QnUpdateProcess::at_installTask_finished(int errorCode) {
    if (errorCode != 0) {
        setAllPeersState(QnPeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::InstallationFailed);
        return;
    }

    finishUpdate(QnUpdateResult::Successful);
}

bool QnUpdateProcess::cancel() {
    if (m_stage == QnFullUpdateStage::Servers)
        return false;

    if (m_currentTask)
        m_currentTask->cancel();
    setAllPeersState(QnPeerUpdateInformation::UpdateCanceled);
    finishUpdate(QnUpdateResult::Cancelled);
    return true;
}

void QnUpdateProcess::removeTemporaryDir() {
    if (m_localTemporaryDir.isEmpty())
        return;

    QDir(m_localTemporaryDir).removeRecursively();
    m_localTemporaryDir = QString();
}
