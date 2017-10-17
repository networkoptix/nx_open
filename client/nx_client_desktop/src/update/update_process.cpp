#include "update_process.h"

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include <common/common_module.h>
#include <common/common_globals.h>
#include <common/static_common_module.h>

#include <client/client_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <update/task/check_update_peer_task.h>
#include <update/task/download_updates_peer_task.h>
#include <update/task/rest_update_peer_task.h>
#include <update/task/check_free_space_peer_task.h>
#include <update/task/upload_updates_peer_task.h>
#include <update/task/install_updates_peer_task.h>
#include <update/low_free_space_warning.h>

#include <utils/applauncher_utils.h>
#include <utils/common/sleep.h>
#include <utils/update/update_utils.h>
#include <utils/common/app_info.h>
#include <nx/utils/log/log.h>

#include <api/runtime_info_manager.h>

namespace {

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

QnPeerUpdateInformation::QnPeerUpdateInformation(const QnMediaServerResourcePtr &server /* = QnMediaServerResourcePtr()*/):
    server(server),
    stage(QnPeerUpdateStage::Init),
    updateInformation(0),
    progress(0)
{
    if (server)
        sourceVersion = server->getVersion();
}

QnUpdateProcess::QnUpdateProcess(const QnUpdateTarget &target):
    base_type(),
    m_target(target),
    m_stage(QnFullUpdateStage::Init),
    m_clientRequiresInstaller(true),
    m_updateResult(QnUpdateResult::Cancelled),
    m_protocolChanged(false)
{
    moveToThread(this);
}

void QnUpdateProcess::pleaseStop()
{
    if (!isRunning())
        return;

    base_type::pleaseStop();
    quit();

    setAllPeersStage(QnPeerUpdateStage::Init);
    setStage(QnFullUpdateStage::Init);
}

void QnUpdateProcess::run() {
    m_failedPeerIds.clear();

    QnCheckForUpdatesPeerTask *checkForUpdatesTask = new QnCheckForUpdatesPeerTask(m_target);
    connect(checkForUpdatesTask,  &QnCheckForUpdatesPeerTask::checkFinished,   this,  [this, checkForUpdatesTask](const QnCheckForUpdateResult &result){
        at_checkForUpdatesTaskFinished(checkForUpdatesTask, result);
    });
    connect(checkForUpdatesTask,  &QnNetworkPeerTask::finished,  checkForUpdatesTask, &QObject::deleteLater);
    m_currentTask = checkForUpdatesTask;
    setStage(QnFullUpdateStage::Check);
    checkForUpdatesTask->start();

    exec();

    if (m_updateResult == QnUpdateResult::Cancelled) {
        if (m_currentTask)
            m_currentTask->cancel();
        setAllPeersStage(QnPeerUpdateStage::Init);
    }

    runtimeInfoManager()->disconnect(this);
    clearUpdateFlag();

    QnUpdateResult result(m_updateResult);
    result.targetVersion = m_target.version;
    result.clientInstallerRequired = m_clientRequiresInstaller;
    result.protocolChanged = m_protocolChanged;

    for (const QnUuid &id: m_failedPeerIds)
    {
        if (auto server = resourcePool()->getIncompatibleServerById(id, true))
            result.failedServers.append(server);
    }

    emit updateFinished(result);
}

void QnUpdateProcess::downloadUpdates() {

    QHash<nx::utils::Url, QString> downloadTargets;
    QMultiHash<nx::utils::Url, QnUuid> peerAssociations;
    QHash<nx::utils::Url, QString> hashByUrl;
    QHash<nx::utils::Url, qint64> fileSizeByUrl;

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        QList<QnUuid> peers = m_idBySystemInformation.values(it.key());
        if (peers.isEmpty())
            continue;

        QString fileName = it.value()->fileName;
        if (fileName.isEmpty())
            fileName = updateFilePath(it.value()->baseFileName);

        if (!fileName.isEmpty() && verifyFile(fileName, it.value()->fileSize, it.value()->md5)) {
            it.value()->fileName = fileName;
            continue;
        }

        downloadTargets.insert(it.value()->url, it.value()->baseFileName);
        hashByUrl.insert(it.value()->url, it.value()->md5);
        fileSizeByUrl.insert(it.value()->url, it.value()->fileSize);
        foreach (const QnUuid &peerId, peers) {
            peerAssociations.insert(it.value()->url, peerId);
            QnPeerUpdateInformation &updateInformation = m_updateInformationById[peerId];
            updateInformation.stage = QnPeerUpdateStage::Download;
            updateInformation.progress = 0;
            emit peerStageChanged(peerId, QnPeerUpdateStage::Download);
        }
    }

    if (qnStaticCommon->engineVersion() != m_target.version && !m_clientRequiresInstaller) {
        QString fileName = m_clientUpdateFile->fileName;
        if (fileName.isEmpty())
            fileName = updateFilePath(m_clientUpdateFile->baseFileName);

        if (!fileName.isEmpty() && verifyFile(fileName, m_clientUpdateFile->fileSize, m_clientUpdateFile->md5)) {
            m_clientUpdateFile->fileName = fileName;
        } else {
            downloadTargets.insert(m_clientUpdateFile->url, m_clientUpdateFile->baseFileName);
            hashByUrl.insert(m_clientUpdateFile->url, m_clientUpdateFile->md5);
            fileSizeByUrl.insert(m_clientUpdateFile->url, m_clientUpdateFile->fileSize);
            peerAssociations.insert(m_clientUpdateFile->url, commonModule()->moduleGUID());
        }
    }


    QnDownloadUpdatesPeerTask *downloadUpdatesPeerTask = new QnDownloadUpdatesPeerTask();
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             this,  [this, downloadUpdatesPeerTask](int errorCode) {
        at_downloadTaskFinished(downloadUpdatesPeerTask, errorCode);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::peerFinished,         this,  [this](const QnUuid &peerId) {
        setPeerStage(peerId, QnPeerUpdateStage::Push);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::progressChanged,      this,  &QnUpdateProcess::progressChanged);
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::peerProgressChanged,  this,  [this](const QnUuid &peerId, int progress) {
        emit peerStageProgressChanged(peerId, QnPeerUpdateStage::Download, progress);
    });
    connect(downloadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             downloadUpdatesPeerTask, &QObject::deleteLater);

    downloadUpdatesPeerTask->setTargets(downloadTargets);
    downloadUpdatesPeerTask->setHashes(hashByUrl);
    downloadUpdatesPeerTask->setFileSizes(fileSizeByUrl);
    downloadUpdatesPeerTask->setPeerAssociations(peerAssociations);
    m_currentTask = downloadUpdatesPeerTask;
    setStage(QnFullUpdateStage::Download);
    downloadUpdatesPeerTask->start(QSet<QnUuid>::fromList(m_updateInformationById.keys()));
}


void QnUpdateProcess::setPeerStage(const QnUuid &peerId, QnPeerUpdateStage stage) {
    if (!m_updateInformationById.contains(peerId))
        return;

    QnPeerUpdateInformation &info = m_updateInformationById[peerId];
    if (info.stage == stage)
        return;

    info.stage = stage;
    emit peerStageChanged(peerId, stage);
}

void QnUpdateProcess::setAllPeersStage(QnPeerUpdateStage stage) {
    foreach(const QnUuid &key, m_updateInformationById.keys())
        setPeerStage(key, stage);
}


void QnUpdateProcess::setCompatiblePeersStage(QnPeerUpdateStage stage) {
    foreach(const QnUuid &key, m_updateInformationById.keys())
        if (!m_incompatiblePeerIds.contains(key))
            setPeerStage(key, stage);
}

void QnUpdateProcess::setIncompatiblePeersStage(QnPeerUpdateStage stage) {
    foreach(const QnUuid &key, m_updateInformationById.keys())
        if (m_incompatiblePeerIds.contains(key))
            setPeerStage(key, stage);
}

void QnUpdateProcess::at_checkForUpdatesTaskFinished(QnCheckForUpdatesPeerTask* task, const QnCheckForUpdateResult &result) {
    switch (result.result)
    {
    case QnCheckForUpdateResult::UpdateFound:
        break;
    case QnCheckForUpdateResult::NoNewerVersion:
        setAllPeersStage(QnPeerUpdateStage::Init);
        finishUpdate(QnUpdateResult::AlreadyUpdated);
        return;
    default:
        setAllPeersStage(QnPeerUpdateStage::Init);
        finishUpdate(QnUpdateResult::DownloadingFailed);
        return;
    }

    m_id = result.version.toString();

    m_target.version = result.version; /* Version can be updated if loading from local file or seeking for latest version. */
    m_clientRequiresInstaller = result.clientInstallerRequired;
    m_updateFiles = task->updateFiles();
    m_clientUpdateFile = task->clientUpdateFile();

    foreach (const QnUuid &serverId, m_target.targets) {
        auto server = resourcePool()->getIncompatibleServerById(serverId, true);
        if (!server)
            continue;

        bool incompatible = (server->getStatus() == Qn::Incompatible);

        if (server->getStatus() != Qn::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        if (!m_target.version.isNull() && server->getVersion() == m_target.version)
            continue;

        QnUuid peerId = server->getId();

        if (!incompatible)
            m_targetPeerIds.insert(peerId);
        else
            m_incompatiblePeerIds.insert(peerId);

        QnPeerUpdateInformation info(server);
        info.updateInformation = m_updateFiles.value(server->getSystemInfo());

        if (!info.updateInformation) {
            NX_ASSERT(0, "No update info for server", Q_FUNC_INFO);
            return;
        }

        info.stage = info.updateInformation->fileName.isEmpty() ? QnPeerUpdateStage::Download
            : QnPeerUpdateStage::Push;
        m_updateInformationById.insert(peerId, info);
        m_idBySystemInformation.insert(server->getSystemInfo(), peerId);
        emit peerStageChanged(peerId, info.stage);
    }

    clearUpdatesCache(m_target.version);

    downloadUpdates();
}

void QnUpdateProcess::at_downloadTaskFinished(QnDownloadUpdatesPeerTask* task, int errorCode)
{
    if (errorCode == QnDownloadUpdatesPeerTask::Cancelled)
    {
        finishUpdate(QnUpdateResult::Cancelled);
        return;
    }

    if (errorCode != 0) {
        setAllPeersStage(QnPeerUpdateStage::Init);
        finishUpdate(errorCode == QnDownloadUpdatesPeerTask::NoFreeSpaceError ? QnUpdateResult::DownloadingFailed_NoFreeSpace : QnUpdateResult::DownloadingFailed);
        return;
    }

    QHash<nx::utils::Url, QString> resultingFiles = task->resultingFiles();

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        if (resultingFiles.contains(it.value()->url))
            it.value()->fileName = resultingFiles[it.value()->url];
    }
    if (!m_clientRequiresInstaller && resultingFiles.contains(m_clientUpdateFile->url))
        m_clientUpdateFile->fileName = resultingFiles[m_clientUpdateFile->url];

    installClientUpdate();
}

void QnUpdateProcess::finishUpdate(QnUpdateResult::Value value) {
    m_updateResult = value;
    stop();
}

void QnUpdateProcess::checkFreeSpace()
{
    setStage(QnFullUpdateStage::CheckFreeSpace);

    for (const auto& targetId: m_targetPeerIds)
    {
        const auto server = resourcePool()->getResourceById<QnMediaServerResource>(targetId);
        if (!server || server->getStatus() != Qn::Online)
            m_failedPeerIds.insert(targetId);
    }

    if (!m_failedPeerIds.isEmpty())
    {
        finishUpdate(QnUpdateResult::UploadingFailed_Offline);
        return;
    }

    QHash<QnSystemInformation, QString> fileBySystemInformation;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it)
        fileBySystemInformation[it.key()] = it.value()->fileName;

    setAllPeersStage(QnPeerUpdateStage::Push);

    auto checkFreeSpacePeerTask = new QnCheckFreeSpacePeerTask();
    checkFreeSpacePeerTask->setUpdateFiles(fileBySystemInformation);
    connect(checkFreeSpacePeerTask, &QnNetworkPeerTask::finished,
        this, &QnUpdateProcess::at_checkFreeSpaceTask_finished);
    connect(checkFreeSpacePeerTask, &QnNetworkPeerTask::finished,
        checkFreeSpacePeerTask, &QObject::deleteLater);
    m_currentTask = checkFreeSpacePeerTask;
    checkFreeSpacePeerTask->start(m_targetPeerIds + m_incompatiblePeerIds);
}

void QnUpdateProcess::installClientUpdate()
{
    using namespace applauncher::api;

    /* Check if we skip this step. */
    if (m_clientRequiresInstaller
        || m_target.denyClientUpdates
        || qnSettings->isClientUpdateDisabled()
        || m_clientUpdateFile->version == qnStaticCommon->engineVersion())
    {
            NX_LOG(lit("Update: QnUpdateProcess: Client update skipped."), cl_logDEBUG1);
            checkFreeSpace();
            return;
    }

    setStage(QnFullUpdateStage::Client);

    NX_LOG(lit("Update: QnUpdateProcess: Installing client update [%1] from %2.")
        .arg(m_target.version.toString())
        .arg(m_clientUpdateFile->fileName), cl_logDEBUG1);
    QFuture<ResultType::Value> future = QtConcurrent::run(&applauncher::installZip,
        m_target.version, m_clientUpdateFile->fileName);

    auto futureWatcher = new QFutureWatcher<ResultType::Value>(this);
    connect(futureWatcher, &QFutureWatcher<ResultType::Value>::finished, this,
        &QnUpdateProcess::at_clientUpdateInstalled);
    connect(futureWatcher, &QFutureWatcher<ResultType::Value>::finished, futureWatcher,
        &QObject::deleteLater);
    futureWatcher->setFuture(future);
}

void QnUpdateProcess::setStage(QnFullUpdateStage stage) {
    if (m_stage == stage)
        return;
    m_stage = stage;
    emit stageChanged(stage);
}

void QnUpdateProcess::at_clientUpdateInstalled()
{
    using namespace applauncher::api;

    auto futureWatcher = dynamic_cast<QFutureWatcher<ResultType::Value>*>(sender());
    NX_ASSERT(futureWatcher);
    if (!futureWatcher)
        return;

    const auto errCode = futureWatcher->result();
    if (errCode != ResultType::ok && errCode != ResultType::alreadyInstalled)
    {
        NX_LOG(lit("Update: QnUpdateProcess: Client update failed with code %1.")
            .arg(QString::fromUtf8(ResultType::toString(errCode))), cl_logERROR);
        setAllPeersStage(QnPeerUpdateStage::Init);
        finishUpdate(QnUpdateResult::ClientInstallationFailed);
        return;
    }

    NX_LOG(lit("Update: QnUpdateProcess: Client update installed with code %1.")
        .arg(QString::fromUtf8(ResultType::toString(errCode))), cl_logINFO);

    checkFreeSpace();
}

void QnUpdateProcess::at_runtimeInfoChanged(const QnPeerRuntimeInfo &data) {
    if (data.uuid == commonModule()->moduleGUID())
        return;

    if (data.data.updateStarted)
        finishUpdate(QnUpdateResult::UploadingFailed);
}

void QnUpdateProcess::installIncompatiblePeers() {
    QHash<QnSystemInformation, QString> targetFiles;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it)
        targetFiles[it.key()] = it.value()->fileName;

    setIncompatiblePeersStage(QnPeerUpdateStage::Install);

    QnRestUpdatePeerTask* restUpdatePeerTask = new QnRestUpdatePeerTask();
    restUpdatePeerTask->setUpdateId(m_id);
    restUpdatePeerTask->setVersion(m_target.version);
    connect(restUpdatePeerTask, &QnNetworkPeerTask::finished,                   this,   &QnUpdateProcess::at_restUpdateTask_finished);
    connect(restUpdatePeerTask, &QnNetworkPeerTask::peerFinished,               this,   [this](const QnUuid &peerId) {
        setPeerStage(peerId, QnPeerUpdateStage::Init);
    });
    connect(restUpdatePeerTask, &QnRestUpdatePeerTask::peerUpdateFinished,      this,   &QnUpdateProcess::at_restUpdateTask_peerUpdateFinished);
    connect(restUpdatePeerTask, &QnNetworkPeerTask::finished,                   restUpdatePeerTask,   &QObject::deleteLater);
    m_currentTask = restUpdatePeerTask;
    setStage(QnFullUpdateStage::Incompatible);
    restUpdatePeerTask->start(m_incompatiblePeerIds);
}

void QnUpdateProcess::at_restUpdateTask_peerUpdateFinished(const QnUuid &incompatibleId, const QnUuid &id) {
    QnPeerUpdateInformation info = m_updateInformationById.take(incompatibleId);
    info.stage = QnPeerUpdateStage::Init;
    info.server = resourcePool()->getResourceById<QnMediaServerResource>(id);
    m_updateInformationById.insert(id, info);
    emit targetsChanged(QSet<QnUuid>::fromList(m_updateInformationById.keys()));
    emit peerStageChanged(id, QnPeerUpdateStage::Init);
}

void QnUpdateProcess::at_restUpdateTask_finished(int errorCode) {
    if (errorCode != 0) {
        setAllPeersStage(QnPeerUpdateStage::Init);
        finishUpdate(QnUpdateResult::RestInstallationFailed);
        return;
    }

    if (m_targetPeerIds.isEmpty()) {
        finishUpdate(QnUpdateResult::Successful);
        return;
    }

    installUpdatesToServers();
}

void QnUpdateProcess::at_checkFreeSpaceTask_finished(
    int errorCode, const QSet<QnUuid>& failedPeers)
{
    if (errorCode == QnCheckFreeSpacePeerTask::NotEnoughFreeSpaceError)
    {
        QnLowFreeSpaceWarning warning;
        warning.failedPeers = failedPeers;

        emit lowFreeSpaceWarning(warning);

        if (!warning.ignore)
        {
            setAllPeersStage(QnPeerUpdateStage::Init);
            finishUpdate(QnUpdateResult::CancelledSilently);
            return;
        }
    }

    prepareToUpload();
}

void QnUpdateProcess::prepareToUpload() {
    setStage(QnFullUpdateStage::Push);

    foreach (const QnUuid &target, m_targetPeerIds) {
        QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(target);
        if (!server || server->getStatus() != Qn::Online)
            m_failedPeerIds.insert(target);
    }

    if (!m_failedPeerIds.isEmpty()) {
        finishUpdate(QnUpdateResult::UploadingFailed_Offline);
        return;
    }

    connect(runtimeInfoManager(),   &QnRuntimeInfoManager::runtimeInfoAdded,    this,   &QnUpdateProcess::at_runtimeInfoChanged);
    connect(runtimeInfoManager(),   &QnRuntimeInfoManager::runtimeInfoChanged,  this,   &QnUpdateProcess::at_runtimeInfoChanged);

    if (!setUpdateFlag()) {
        finishUpdate(QnUpdateResult::LockFailed);
        return;
    }

    uploadUpdatesToServers();
}

bool QnUpdateProcess::setUpdateFlag() {
    for (const QnPeerRuntimeInfo &runtimeInfo: runtimeInfoManager()->items()->getItems())
    {
        if (runtimeInfo.data.updateStarted)
            return false;
    }
    QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
    localInfo.data.updateStarted = true;
    runtimeInfoManager()->updateLocalItem(localInfo);
    return true;
}

void QnUpdateProcess::clearUpdateFlag() {
    QnPeerRuntimeInfo runtimeInfo = runtimeInfoManager()->localInfo();
    if (runtimeInfo.data.updateStarted) {
        runtimeInfo.data.updateStarted = false;
        runtimeInfoManager()->updateLocalItem(runtimeInfo);
    }
}

void QnUpdateProcess::uploadUpdatesToServers() {
    QHash<QnSystemInformation, QString> fileBySystemInformation;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it)
        fileBySystemInformation[it.key()] = it.value()->fileName;

    setAllPeersStage(QnPeerUpdateStage::Push);

    QnUploadUpdatesPeerTask* uploadUpdatesPeerTask = new QnUploadUpdatesPeerTask();
    uploadUpdatesPeerTask->setUpdateId(m_id);
    uploadUpdatesPeerTask->setUploads(fileBySystemInformation);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             this,     &QnUpdateProcess::at_uploadTask_finished);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::peerFinished,         this,     [this](const QnUuid &peerId) {
            setPeerStage(peerId, QnPeerUpdateStage::Install);
    });
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::progressChanged,      this,     &QnUpdateProcess::progressChanged);
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::peerProgressChanged,  this,     [this](const QnUuid &peerId, int progress) {
        emit peerStageProgressChanged(peerId, QnPeerUpdateStage::Push, progress);
    });
    connect(uploadUpdatesPeerTask,  &QnNetworkPeerTask::finished,             uploadUpdatesPeerTask,     &QObject::deleteLater);
    m_currentTask = uploadUpdatesPeerTask;
    uploadUpdatesPeerTask->start(m_targetPeerIds + m_incompatiblePeerIds);

    QPointer<QnUploadUpdatesPeerTask> uploadUpdatesPeerTaskPtr(uploadUpdatesPeerTask);

    connect(resourcePool(), &QnResourcePool::statusChanged, this, [this, uploadUpdatesPeerTaskPtr](const QnResourcePtr &resource) {
        if (m_stage != QnFullUpdateStage::Push)
            return;

        if (!m_targetPeerIds.contains(resource->getId()))
            return;

        if (uploadUpdatesPeerTaskPtr.isNull())
            return;

        if (resource->getStatus() != Qn::Online) {
            uploadUpdatesPeerTaskPtr->cancel();
            m_failedPeerIds.insert(resource->getId());
            finishUpdate(QnUpdateResult::UploadingFailed_Offline);
        }
    });
}

void QnUpdateProcess::installUpdatesToServers() {
    QnInstallUpdatesPeerTask* installUpdatesPeerTask = new QnInstallUpdatesPeerTask();
    installUpdatesPeerTask->setUpdateId(m_id);
    installUpdatesPeerTask->setVersion(m_target.version);
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::finished,                   this,   &QnUpdateProcess::at_installTask_finished);
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::peerFinished,               this,   [this](const QnUuid &peerId) {
        setPeerStage(peerId, QnPeerUpdateStage::Init);
    });
    connect(installUpdatesPeerTask,  &QnNetworkPeerTask::progressChanged,           this,     &QnUpdateProcess::progressChanged);
    connect(installUpdatesPeerTask,  &QnNetworkPeerTask::peerProgressChanged,       this,     [this](const QnUuid &peerId, int progress) {
        emit peerStageProgressChanged(peerId, QnPeerUpdateStage::Install, progress);
    });
    connect(installUpdatesPeerTask, &QnNetworkPeerTask::finished,                   installUpdatesPeerTask,   &QObject::deleteLater);
    connect(installUpdatesPeerTask, &QnInstallUpdatesPeerTask::protocolProblemDetected, this,   [this](){ m_protocolChanged = true; });
    setStage(QnFullUpdateStage::Servers);
    setCompatiblePeersStage(QnPeerUpdateStage::Install);
    installUpdatesPeerTask->start(m_targetPeerIds);
}

void QnUpdateProcess::at_uploadTask_finished(int errorCode, const QSet<QnUuid> &failedPeers) {
    if (errorCode != 0) {
        setAllPeersStage(QnPeerUpdateStage::Init);
        m_failedPeerIds = failedPeers;
        switch (errorCode) {
        case QnUploadUpdatesPeerTask::NoFreeSpaceError:
            finishUpdate(QnUpdateResult::UploadingFailed_NoFreeSpace);
            break;
        case QnUploadUpdatesPeerTask::TimeoutError:
            finishUpdate(QnUpdateResult::UploadingFailed_Timeout);
            break;
        case QnUploadUpdatesPeerTask::AuthenticationError:
            finishUpdate(QnUpdateResult::UploadingFailed_AuthenticationFailed);
            break;
        default:
            finishUpdate(QnUpdateResult::UploadingFailed);
            break;
        }
        return;
    }

    installIncompatiblePeers();
}


void QnUpdateProcess::at_installTask_finished(int errorCode) {
    if (errorCode != 0) {
        setAllPeersStage(QnPeerUpdateStage::Init);
        finishUpdate(QnUpdateResult::InstallationFailed);
        return;
    }

    finishUpdate(QnUpdateResult::Successful);
}
