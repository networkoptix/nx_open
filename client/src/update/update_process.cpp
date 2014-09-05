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

QnUpdateProcess::QnUpdateProcess(const QnMediaServerResourceList &targets, QObject *parent):
    base_type(parent),
    id(QUuid::createUuid()),
    m_stage(QnFullUpdateStage::Init),
    m_distributedMutex(NULL)
{
    foreach (const QnMediaServerResourcePtr &server, targets) {
        bool incompatible = (server->getStatus() == Qn::Incompatible);

        if (server->getStatus() != Qn::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        if (!targetVersion.isNull() && server->getVersion() == targetVersion)
            continue;

        QUuid peerId = server->getId();

        m_targetPeerIds.insert(peerId);
        if (incompatible)
            m_incompatiblePeerIds.insert(peerId);

        QnPeerUpdateInformation info(server);
        info.updateInformation = updateFiles.value(server->getSystemInfo());

        if (!info.updateInformation) {
            Q_ASSERT_X(0, "No update info for server", Q_FUNC_INFO);
            return;
        }

        info.state = info.updateInformation->fileName.isEmpty() ? QnPeerUpdateInformation::PendingDownloading
            : QnPeerUpdateInformation::PendingUpload;

        updateInformationById.insert(peerId, info);
        idBySystemInformation.insert(server->getSystemInfo(), peerId);
    }
}

void QnUpdateProcess::downloadUpdates() {

    QHash<QUrl, QString> downloadTargets;
    QMultiHash<QUrl, QUuid> peerAssociations;
    QHash<QUrl, QString> hashByUrl;
    QHash<QUrl, qint64> fileSizeByUrl;

    for (auto it = updateFiles.begin(); it != updateFiles.end(); ++it) {
        QList<QUuid> peers = idBySystemInformation.values(it.key());
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
            QnPeerUpdateInformation &updateInformation = updateInformationById[peerId];
            updateInformation.state = QnPeerUpdateInformation::UpdateDownloading;
            updateInformation.progress = 0;
        }
    }

    if (!clientRequiresInstaller) {
        QString fileName = clientUpdateFile->fileName;
        if (fileName.isEmpty())
            fileName = updateFilePath(updatesDirName, clientUpdateFile->baseFileName);

        if (!fileName.isEmpty() && verifyFile(fileName, clientUpdateFile->fileSize, clientUpdateFile->md5)) {
            clientUpdateFile->fileName = fileName;
        } else {
            downloadTargets.insert(clientUpdateFile->url, clientUpdateFile->baseFileName);
            hashByUrl.insert(clientUpdateFile->url, clientUpdateFile->md5);
            fileSizeByUrl.insert(clientUpdateFile->url, clientUpdateFile->fileSize);
            peerAssociations.insert(clientUpdateFile->url, qnCommon->moduleGUID());
        }
    }


    QnDownloadUpdatesPeerTask *downloadUpdatesPeerTask = new QnDownloadUpdatesPeerTask();
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             this,  [this, downloadUpdatesPeerTask](int errorCode) {
        at_downloadTaskFinished(downloadUpdatesPeerTask, errorCode);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::peerFinished,         this,  [this](const QUuid &peerId) {
        setPeerState(peerId, QnPeerUpdateInformation::PendingUpload);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::progressChanged,      this,  &QnUpdateProcess::taskProgressChanged);
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::peerProgressChanged,  this,  &QnUpdateProcess::networkTask_peerProgressChanged);
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             downloadUpdatesPeerTask, &QObject::deleteLater);

    downloadUpdatesPeerTask->setTargetDir(updatesDirName);
    downloadUpdatesPeerTask->setTargets(downloadTargets);
    downloadUpdatesPeerTask->setHashes(hashByUrl);
    downloadUpdatesPeerTask->setFileSizes(fileSizeByUrl);
    downloadUpdatesPeerTask->setPeerAssociations(peerAssociations);
    m_currentTask = downloadUpdatesPeerTask;
    setStage(QnFullUpdateStage::Download);
    downloadUpdatesPeerTask->start(QSet<QUuid>::fromList(updateInformationById.keys()));
}

void QnUpdateProcess::setPeerState(const QUuid &peerId, QnPeerUpdateInformation::State state) {
    auto it = updateInformationById.find(peerId);
    if (it == updateInformationById.end())
        return;

    if (it->state != state) {
        it->state = state;
        emit peerChanged(peerId);
    }
}


void QnUpdateProcess::setAllPeersState(QnPeerUpdateInformation::State state) {
    for (auto it = updateInformationById.begin(); it != updateInformationById.end(); ++it) {
        if (it->state == state)
            continue;

        it->state = state;
        emit peerChanged(it.key());
    }
}


void QnUpdateProcess::setPeersState(QnPeerUpdateInformation::State state) {
    for (auto it = updateInformationById.begin(); it != updateInformationById.end(); ++it) {
        if (m_incompatiblePeerIds.contains(it.key()))
            continue;

        if (it->state == state)
            continue;

        it->state = state;
        emit peerChanged(it.key());
    }
}

void QnUpdateProcess::setIncompatiblePeersState(QnPeerUpdateInformation::State state) {
    for (auto it = updateInformationById.begin(); it != updateInformationById.end(); ++it) {
        if (!m_incompatiblePeerIds.contains(it.key()))
            continue;

        if (it->state == state)
            continue;

        it->state = state;
        emit peerChanged(it.key());
    }
}

void QnUpdateProcess::at_downloadTaskFinished(QnDownloadUpdatesPeerTask* task, int errorCode) {
    if (errorCode != 0) {
        setAllPeersState(QnPeerUpdateInformation::UpdateFailed);
        finishUpdate(QnUpdateResult::DownloadingFailed);
        return;
    }

    QHash<QUrl, QString> resultingFiles = task->resultingFiles();

    for (auto it = updateFiles.begin(); it != updateFiles.end(); ++it) {
        if (resultingFiles.contains(it.value()->url))
            it.value()->fileName = resultingFiles[it.value()->url];
    }
    if (!clientRequiresInstaller && resultingFiles.contains(clientUpdateFile->url))
        clientUpdateFile->fileName = resultingFiles[clientUpdateFile->url];

    installClientUpdate();
}

void QnUpdateProcess::finishUpdate(QnUpdateResult result) {
    unlockMutex();
    emit updateFinished(result);
}

void QnUpdateProcess::installClientUpdate() {
    /* Check if we skip this step. */
    if (clientRequiresInstaller 
        || disableClientUpdates 
        || qnSettings->isClientUpdateDisabled() 
        || clientUpdateFile->version == qnCommon->engineVersion()) {
            installIncompatiblePeers();
            return;
    }

    setStage(QnFullUpdateStage::Client);

    QFuture<applauncher::api::ResultType::Value> future = QtConcurrent::run(&applauncher::installZip, targetVersion, clientUpdateFile->fileName);
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
    for (auto it = updateFiles.begin(); it != updateFiles.end(); ++it) {
        targetFiles[it.key()] = it.value()->fileName;
        setIncompatiblePeersState(QnPeerUpdateInformation::UpdateInstalling);
    }

    QnRestUpdatePeerTask* restUpdatePeerTask = new QnRestUpdatePeerTask();
    restUpdatePeerTask->setUpdateId(id.toString());
    restUpdatePeerTask->setUpdateFiles(targetFiles);
    restUpdatePeerTask->setVersion(targetVersion);
    connect(restUpdatePeerTask, &QnNetworkPeerTask::finished,                   this,   &QnUpdateProcess::at_restUpdateTask_finished);
    connect(restUpdatePeerTask, &QnRestUpdatePeerTask::peerUpdateFinished,      this,   &QnUpdateProcess::at_restUpdateTask_peerUpdateFinished);
    connect(restUpdatePeerTask, &QnNetworkPeerTask::finished,                   restUpdatePeerTask,   &QObject::deleteLater);
    m_currentTask = restUpdatePeerTask;
    setStage(QnFullUpdateStage::Incompatible);
    restUpdatePeerTask->start(m_incompatiblePeerIds);
}

void QnUpdateProcess::at_restUpdateTask_peerUpdateFinished(const QUuid &incompatibleId, const QUuid &id) {
    QnPeerUpdateInformation info = updateInformationById.take(incompatibleId);
    info.state = QnPeerUpdateInformation::UpdateFinished;
    info.server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
    updateInformationById.insert(id, info);
    emit targetsChanged(QSet<QUuid>::fromList(updateInformationById.keys()));
    emit peerChanged(id);
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
    for (auto it = updateFiles.begin(); it != updateFiles.end(); ++it)
        fileBySystemInformation[it.key()] = it.value()->fileName;

    setPeersState(QnPeerUpdateInformation::UpdateUploading);

    QnUploadUpdatesPeerTask* uploadUpdatesPeerTask = new QnUploadUpdatesPeerTask();
    uploadUpdatesPeerTask->setUpdateId(id.toString());
    uploadUpdatesPeerTask->setUploads(fileBySystemInformation);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             this,     &QnUpdateProcess::at_uploadTask_finished);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::peerFinished,         this,     [this](const QUuid &peerId) {
            setPeerState(peerId, QnPeerUpdateInformation::PendingInstallation);
    });
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::progressChanged,      this,     &QnUpdateProcess::taskProgressChanged);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::peerProgressChanged,  this,     &QnUpdateProcess::networkTask_peerProgressChanged);
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
    installUpdatesPeerTask->setUpdateId(id.toString());
    installUpdatesPeerTask->setVersion(targetVersion);
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::finished,                   this,   &QnUpdateProcess::at_installTask_finished);
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::peerFinished,               this,   [this](const QUuid &peerId) {
        setPeerState(peerId, QnPeerUpdateInformation::UpdateFinished);
    });
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::finished,                   installUpdatesPeerTask,   &QObject::deleteLater);
    setStage(QnFullUpdateStage::Servers);
    setPeersState(QnPeerUpdateInformation::UpdateInstalling);
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
