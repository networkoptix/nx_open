#include "media_server_update_tool.h"

#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <utils/update/update_utils.h>
#include <update/task/check_update_peer_task.h>

#include <client/client_settings.h>

#include <version.h>

namespace {

    const QString QN_UPDATE_PACKAGE_URL = lit("http://enk.me/bg/dklychkov/exmaple_update/get_update");
    

    #ifdef Q_OS_MACX
    const bool defaultDisableClientUpdates = true;
    #else
    const bool defaultDisableClientUpdates = false;
    #endif

} // anonymous namespace

// TODO: #dklychkov Split this class to a set of QnNetworkPeerTask

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_tasksThread(new QThread(this)),
    m_state(Idle),
    m_targetMustBeNewer(true),
    m_updateProcess(NULL),
    m_checkForUpdatesPeerTask(new QnCheckForUpdatesPeerTask()),
    m_disableClientUpdates(defaultDisableClientUpdates)
{
    m_checkForUpdatesPeerTask->setUpdatesUrl(QUrl(qnSettings->updateFeedUrl()));

    m_checkForUpdatesPeerTask->moveToThread(m_tasksThread);

    connect(m_checkForUpdatesPeerTask,                  &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_checkForUpdatesTask_finished);

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

    if (m_updateProcess)
        delete m_updateProcess;
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
    removeTemporaryDir();
    setState(Idle);
    emit updateFinished(result);
}

QnSoftwareVersion QnMediaServerUpdateTool::targetVersion() const {
    return m_updateProcess->targetVersion;
}

QnPeerUpdateInformation QnMediaServerUpdateTool::updateInformation(const QUuid &peerId) const {
    auto it = m_updateProcess->updateInformationById.find(peerId);
    if (it != m_updateProcess->updateInformationById.end())
        return it.value();

    QnPeerUpdateInformation info(qnResPool->getIncompatibleResourceById(peerId, true).dynamicCast<QnMediaServerResource>());
    if (info.server && m_state == Idle) {
        info.updateInformation = m_updateProcess->updateFiles[info.server->getSystemInfo()];
        if (m_updateProcess->targetVersion.isNull())
            info.state = QnPeerUpdateInformation::UpdateUnknown;
        else
            info.state = info.updateInformation ? QnPeerUpdateInformation::UpdateFound : QnPeerUpdateInformation::UpdateNotFound;
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

    if (m_updateProcess)
        delete m_updateProcess;
    m_updateProcess = NULL;

    m_disableClientUpdates = defaultDisableClientUpdates;
}

bool QnMediaServerUpdateTool::isClientRequiresInstaller() const {
    return m_updateProcess->clientRequiresInstaller;
}

void QnMediaServerUpdateTool::checkForUpdates(const QnSoftwareVersion &version, bool denyMajorUpdates) {
    if (m_state >= CheckingForUpdates)
        return;

    setState(CheckingForUpdates);

    QSet<QUuid> peers;
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        peers.insert(server->getId());

    
    m_tasksThread->start();
    m_checkForUpdatesPeerTask->setTargetVersion(version);
    m_checkForUpdatesPeerTask->setDisableClientUpdates(m_disableClientUpdates || qnSettings->isClientUpdateDisabled());
    m_checkForUpdatesPeerTask->setDenyMajorUpdates(denyMajorUpdates);
    m_checkForUpdatesPeerTask->start(peers);
}

void QnMediaServerUpdateTool::checkForUpdates(const QString &fileName) {
    if (m_state >= CheckingForUpdates)
        return;

    setState(CheckingForUpdates);

    QSet<QUuid> peers;
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        peers.insert(server->getId());

    m_tasksThread->start();
    m_checkForUpdatesPeerTask->setUpdateFileName(fileName);
    m_checkForUpdatesPeerTask->setDisableClientUpdates(m_disableClientUpdates || qnSettings->isClientUpdateDisabled());
    m_checkForUpdatesPeerTask->setDenyMajorUpdates(false);
    m_checkForUpdatesPeerTask->start(peers);
}

void QnMediaServerUpdateTool::removeTemporaryDir() {
    if (m_updateProcess->localTemporaryDir.isEmpty())
        return;

    QDir(m_updateProcess->localTemporaryDir).removeRecursively();
    m_updateProcess->localTemporaryDir = QString();
}

void QnMediaServerUpdateTool::updateServers() {
    if (m_state != Idle)
        return;

    if (m_updateProcess)
        delete m_updateProcess;

    m_updateProcess = new QnUpdateProcess(actualTargets());
    m_updateProcess->updateFiles = m_checkForUpdatesPeerTask->updateFiles();
    m_updateProcess->localTemporaryDir = m_checkForUpdatesPeerTask->temporaryDir();
    m_updateProcess->targetVersion = m_checkForUpdatesPeerTask->targetVersion();
    m_updateProcess->clientRequiresInstaller = m_checkForUpdatesPeerTask->isClientRequiresInstaller();
    m_updateProcess->clientUpdateFile = m_checkForUpdatesPeerTask->clientUpdateFile();
    m_updateProcess->disableClientUpdates = m_checkForUpdatesPeerTask->isDisableClientUpdates();

    connect(m_updateProcess, &QnUpdateProcess::peerChanged,                     this, &QnMediaServerUpdateTool::peerChanged);
    connect(m_updateProcess, &QnUpdateProcess::taskProgressChanged,             this, &QnMediaServerUpdateTool::at_taskProgressChanged);
    connect(m_updateProcess, &QnUpdateProcess::networkTask_peerProgressChanged, this, &QnMediaServerUpdateTool::at_networkTask_peerProgressChanged);
    connect(m_updateProcess, &QnUpdateProcess::updateFinished,                  this, &QnMediaServerUpdateTool::finishUpdate);
    connect(m_updateProcess, &QnUpdateProcess::targetsChanged,                  this, &QnMediaServerUpdateTool::targetsChanged);
    connect(m_updateProcess, &QnUpdateProcess::stageChanged,                    this, [this](QnFullUpdateStage stage) {
        QnMediaServerUpdateTool::State state = Idle;
        switch (stage) {
        case QnFullUpdateStage::Check:
            state = CheckingForUpdates;
            break;
        case QnFullUpdateStage::Download:
            state = DownloadingUpdate;
            break;
        case QnFullUpdateStage::Client:
            state = InstallingClientUpdate;
            break;
        case QnFullUpdateStage::Incompatible:
            state = InstallingToIncompatiblePeers;
            break;
        case QnFullUpdateStage::Push:
            state = UploadingUpdate;
            break;
        case QnFullUpdateStage::Servers:
            state = InstallingUpdate;
            break;
        default:
            break;
        }
        setState(state);
    });

    m_tasksThread->start();
    m_updateProcess->downloadUpdates();
}

bool QnMediaServerUpdateTool::cancelUpdate() {
    if (!m_updateProcess)
        return true;
    return m_updateProcess->cancel();
}

void QnMediaServerUpdateTool::at_checkForUpdatesTask_finished(int errorCode) {
    m_tasksThread->quit();

    m_updateProcess->updateFiles = m_checkForUpdatesPeerTask->updateFiles();
    m_updateProcess->localTemporaryDir = m_checkForUpdatesPeerTask->temporaryDir();
    m_updateProcess->targetVersion = m_checkForUpdatesPeerTask->targetVersion();
    m_updateProcess->clientRequiresInstaller = m_checkForUpdatesPeerTask->isClientRequiresInstaller();
    m_updateProcess->clientUpdateFile = m_checkForUpdatesPeerTask->clientUpdateFile();

    setState(Idle);
    emit checkForUpdatesFinished(static_cast<QnCheckForUpdateResult>(errorCode));
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

    switch (m_updateProcess->updateInformationById[peerId].state) {
    case QnPeerUpdateInformation::PendingUpload:
    case QnPeerUpdateInformation::UpdateUploading:
        stage = QnPeerUpdateStage::Push;
        break;
    case QnPeerUpdateInformation::PendingInstallation:
    case QnPeerUpdateInformation::UpdateInstalling:
        stage = QnPeerUpdateStage::Install;
        progress = 50;
        break;
    default:
        break;
    }

    m_updateProcess->updateInformationById[peerId].progress = (progress + static_cast<int>(stage)*100) / ( static_cast<int>(QnPeerUpdateStage::Count) ) ;
    emit peerChanged(peerId);
}
