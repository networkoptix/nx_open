#include "media_server_update_tool.h"

#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#include <QtConcurrent>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <utils/update/update_utils.h>
#include <update/task/check_update_peer_task.h>

#include <client/client_settings.h>

#include <utils/common/app_info.h>

namespace {

    const QString QN_UPDATE_PACKAGE_URL = lit("http://hdw.mx/upcombiner/upcombine");
    

    #ifdef Q_OS_MACX
    const bool defaultDisableClientUpdates = true;
    #else
    const bool defaultDisableClientUpdates = false;
    #endif

    QnSoftwareVersion getCurrentVersion() {
        QnSoftwareVersion minimalVersion = qnCommon->engineVersion();
        foreach (const QnMediaServerResourcePtr &server, qnResPool->getAllServers()) {
            if (server->getVersion() < minimalVersion)
                minimalVersion = server->getVersion();
        }
        return minimalVersion;
    }

} // anonymous namespace

// TODO: #dklychkov Split this class to a set of QnNetworkPeerTask

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_stage(QnFullUpdateStage::Init),
    m_updateProcess(NULL),
    m_disableClientUpdates(defaultDisableClientUpdates)
{
    auto targetsWatcher = [this] {
        if (!m_targets.isEmpty())
            return;
        emit targetsChanged(actualTargetIds());
    };
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   targetsWatcher);
    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   targetsWatcher);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   targetsWatcher);
}

QnMediaServerUpdateTool::~QnMediaServerUpdateTool() {
    if (m_updateProcess) {
        m_updateProcess->stop();
        delete m_updateProcess;
    }
}

QnFullUpdateStage QnMediaServerUpdateTool::stage() const {
    return m_stage;
}

bool QnMediaServerUpdateTool::isUpdating() const {
    return m_updateProcess;
}

bool QnMediaServerUpdateTool::idle() const {
    return m_stage == QnFullUpdateStage::Init;
}

void QnMediaServerUpdateTool::setStage(QnFullUpdateStage stage) {
    if (m_stage == stage)
        return;

    m_stage = stage;
    emit stageChanged(stage);

    setStageProgress(0);
}

void QnMediaServerUpdateTool::setStageProgress(int progress) {
    emit stageProgressChanged(m_stage, progress);    
}


void QnMediaServerUpdateTool::setPeerStage(const QUuid &peerId, QnPeerUpdateStage stage) {
    emit peerStageChanged(peerId, stage);
    setPeerStageProgress(peerId, stage, 0);
}

void QnMediaServerUpdateTool::setPeerStageProgress(const QUuid &peerId, QnPeerUpdateStage stage, int progress) {
    emit peerStageProgressChanged(peerId, stage, progress);
}

void QnMediaServerUpdateTool::finishUpdate(const QnUpdateResult &result) {
    emit updateFinished(result);
    setStage(QnFullUpdateStage::Init);
}

void QnMediaServerUpdateTool::updateTargets() {
    if (!m_targets.isEmpty())
        return;

    emit targetsChanged(actualTargetIds());
}

QnMediaServerResourceList QnMediaServerUpdateTool::targets() const {
    return m_targets;
}

void QnMediaServerUpdateTool::setTargets(const QSet<QUuid> &targets, bool client) {
    m_targets.clear();

    QSet<QUuid> suitableTargets;

    foreach (const QUuid &id, targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
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

    QnMediaServerResourceList result;
    foreach (const QnMediaServerResourcePtr &server, qnResPool->getResourcesWithFlag(Qn::server).filtered<QnMediaServerResource>()) {
        if (server->getStatus() == Qn::Online)
            result.append(server);
    }

    foreach (const QnMediaServerResourcePtr &server, qnResPool->getAllIncompatibleResources().filtered<QnMediaServerResource>()) {
        if (server->getSystemName() == qnCommon->localSystemName() && server->getStatus() == Qn::Incompatible)
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
        query.addQueryItem(lit("version"), qnCommon->engineVersion().toString());
        query.addQueryItem(lit("current"), getCurrentVersion().toString());
    } else {
        query.addQueryItem(lit("version"), targetVersion.toString());
        query.addQueryItem(lit("password"), passwordForBuild(static_cast<unsigned>(targetVersion.build())));
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

void QnMediaServerUpdateTool::checkForUpdates(const QnSoftwareVersion &version, bool denyMajorUpdates, std::function<void(const QnCheckForUpdateResult &result)> func) {
    QnUpdateTarget target(actualTargetIds(), version, m_disableClientUpdates || qnSettings->isClientUpdateDisabled(), denyMajorUpdates);
    checkForUpdates(target, func);
}

void QnMediaServerUpdateTool::checkForUpdates(const QString &fileName, std::function<void(const QnCheckForUpdateResult &result)> func) {
    QnUpdateTarget target(actualTargetIds(), fileName, m_disableClientUpdates || qnSettings->isClientUpdateDisabled());
    checkForUpdates(target, func);
}


void QnMediaServerUpdateTool::startUpdate(const QnSoftwareVersion &version /*= QnSoftwareVersion()*/, bool denyMajorUpdates /*= false*/) {
    QnUpdateTarget target(actualTargetIds(), version, m_disableClientUpdates || qnSettings->isClientUpdateDisabled(), denyMajorUpdates);
    startUpdate(target);
}

void QnMediaServerUpdateTool::startUpdate(const QString &fileName) {
    QnUpdateTarget target(actualTargetIds(), fileName, m_disableClientUpdates || qnSettings->isClientUpdateDisabled());
    startUpdate(target);
}

bool QnMediaServerUpdateTool::cancelUpdate() {
    if (!m_updateProcess)
        return true;

    if (m_stage == QnFullUpdateStage::Servers)
        return false;

    setTargets(QSet<QUuid>(), defaultDisableClientUpdates);
    m_updateProcess->pleaseStop();
    return true;
}

void QnMediaServerUpdateTool::checkForUpdates(const QnUpdateTarget &target, std::function<void(const QnCheckForUpdateResult &result)> func) {
    QnCheckForUpdatesPeerTask *checkForUpdatesTask = new QnCheckForUpdatesPeerTask(target);
    if (func)
        connect(checkForUpdatesTask,  &QnCheckForUpdatesPeerTask::checkFinished,  this,  [this, func](const QnCheckForUpdateResult &result){
            func(result);
        });
    else
        connect(checkForUpdatesTask,  &QnCheckForUpdatesPeerTask::checkFinished,  this,  &QnMediaServerUpdateTool::checkForUpdatesFinished);
    connect(checkForUpdatesTask,  &QnNetworkPeerTask::finished,             checkForUpdatesTask, &QObject::deleteLater);
    QtConcurrent::run(checkForUpdatesTask, &QnCheckForUpdatesPeerTask::start);
    //checkForUpdatesTask->start();
}


void QnMediaServerUpdateTool::startUpdate(const QnUpdateTarget &target) {
    if (m_updateProcess)
        return;

    if (m_targets.isEmpty())
        setTargets(target.targets, defaultDisableClientUpdates);

    m_updateProcess = new QnUpdateProcess(target);
    connect(m_updateProcess, &QnUpdateProcess::stageChanged,                    this, &QnMediaServerUpdateTool::setStage);
    connect(m_updateProcess, &QnUpdateProcess::progressChanged,                 this, &QnMediaServerUpdateTool::setStageProgress);
    connect(m_updateProcess, &QnUpdateProcess::peerStageChanged,                this, &QnMediaServerUpdateTool::setPeerStage);
    connect(m_updateProcess, &QnUpdateProcess::peerStageProgressChanged,        this, &QnMediaServerUpdateTool::setPeerStageProgress);
    connect(m_updateProcess, &QnUpdateProcess::updateFinished,                  this, &QnMediaServerUpdateTool::finishUpdate);
    connect(m_updateProcess, &QnUpdateProcess::targetsChanged,                  this, &QnMediaServerUpdateTool::targetsChanged);
    connect(m_updateProcess, &QThread::finished, this, [this]{
        m_updateProcess->deleteLater();
        m_updateProcess = NULL;
        setTargets(QSet<QUuid>(), defaultDisableClientUpdates);
    });

    m_updateProcess->start();
}

