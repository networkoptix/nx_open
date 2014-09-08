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
    m_stage(QnFullUpdateStage::Init),
    m_updateProcess(NULL),
    m_disableClientUpdates(defaultDisableClientUpdates)
{
    auto targetsWatcher = [this] {
        if (!m_targets.isEmpty())
            return;
        emit targetsChanged(actualTargetIds());
    };

    connect(qnResPool,  &QnResourcePool::resourceAdded, this, targetsWatcher);
    connect(qnResPool,  &QnResourcePool::resourceRemoved, this, targetsWatcher);
}

QnMediaServerUpdateTool::~QnMediaServerUpdateTool() {
    if (m_updateProcess)
        delete m_updateProcess;
}

QnFullUpdateStage QnMediaServerUpdateTool::stage() const {
    return m_stage;
}

bool QnMediaServerUpdateTool::isUpdating() const {
    return m_stage >= QnFullUpdateStage::Download;
}

bool QnMediaServerUpdateTool::idle() const {
    return m_stage == QnFullUpdateStage::Init;
}

void QnMediaServerUpdateTool::setStage(QnFullUpdateStage stage) {
    if (m_stage == stage)
        return;

    m_stage = stage;
    emit stageChanged(stage);

    int offset = 0; // infinite stages

    switch (m_stage) {
    case QnFullUpdateStage::Download:
        break;
    case QnFullUpdateStage::Client:
        offset = 50;
        break;
    case QnFullUpdateStage::Incompatible:
        offset = 50;
        break;
    case QnFullUpdateStage::Push:
        break;
    case QnFullUpdateStage::Servers:
        offset = 50;
        break;
    default:
        return;
    }

    int progress = (static_cast<int>(stage)*100 + offset) / ( static_cast<int>(QnFullUpdateStage::Count) );
    emit progressChanged(progress);    
}

void QnMediaServerUpdateTool::finishUpdate(QnUpdateResult result) {
    setStage(QnFullUpdateStage::Init);
    emit updateFinished(result);
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
    m_targetPeerIds.clear();
    m_incompatiblePeerIds.clear();
    m_targetPeerIds.clear();
    m_incompatiblePeerIds.clear();
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
    return m_updateProcess->cancel();
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
    checkForUpdatesTask->start();
}


void QnMediaServerUpdateTool::startUpdate(const QnUpdateTarget &target) {
    if (m_updateProcess)
        delete m_updateProcess;

    m_updateProcess = new QnUpdateProcess(target);
    connect(m_updateProcess, &QnUpdateProcess::stageChanged,                    this, &QnMediaServerUpdateTool::setStage);
    connect(m_updateProcess, &QnUpdateProcess::progressChanged,                 this, &QnMediaServerUpdateTool::progressChanged);
    connect(m_updateProcess, &QnUpdateProcess::peerStageChanged,                this, &QnMediaServerUpdateTool::peerStageChanged);
    connect(m_updateProcess, &QnUpdateProcess::peerProgressChanged,             this, &QnMediaServerUpdateTool::peerProgressChanged);
    connect(m_updateProcess, &QnUpdateProcess::updateFinished,                  this, &QnMediaServerUpdateTool::finishUpdate);
    connect(m_updateProcess, &QnUpdateProcess::targetsChanged,                  this, &QnMediaServerUpdateTool::targetsChanged);

    m_updateProcess->start();
}

