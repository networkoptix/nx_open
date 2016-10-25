#include "media_server_update_tool.h"

#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#include <QtConcurrent/QtConcurrent>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/incompatible_server_watcher.h>
#include <common/common_module.h>
#include <utils/update/update_utils.h>
#include <update/task/check_update_peer_task.h>
#include <update/low_free_space_warning.h>

#include <client/client_settings.h>
#include <client/desktop_client_message_processor.h>

#include <utils/common/app_info.h>
#include <core/resource/fake_media_server.h>
#include <api/global_settings.h>

namespace {

    const bool defaultEnableClientUpdates = true;

    QnSoftwareVersion getCurrentVersion()
    {
        QnSoftwareVersion minimalVersion = qnCommon->engineVersion();
        const auto allServers = qnResPool->getAllServers(Qn::AnyStatus);
        for(const QnMediaServerResourcePtr &server: allServers)
        {
            if (server->getVersion() < minimalVersion)
                minimalVersion = server->getVersion();
        }
        return minimalVersion;
    }

} // anonymous namespace

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_stage(QnFullUpdateStage::Init),
    m_updateProcess(NULL),
    m_enableClientUpdates(defaultEnableClientUpdates)
{
    auto targetsWatcher = [this](const QnResourcePtr &resource) {
        if (!resource->hasFlags(Qn::server))
            return;

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


void QnMediaServerUpdateTool::setPeerStage(const QnUuid &peerId, QnPeerUpdateStage stage) {
    emit peerStageChanged(peerId, stage);
    setPeerStageProgress(peerId, stage, 0);
}

void QnMediaServerUpdateTool::setPeerStageProgress(const QnUuid &peerId, QnPeerUpdateStage stage, int progress) {
    emit peerStageProgressChanged(peerId, stage, progress);
}

void QnMediaServerUpdateTool::finishUpdate(const QnUpdateResult &result) {
    emit updateFinished(result);
    setStage(QnFullUpdateStage::Init);
}

QnMediaServerResourceList QnMediaServerUpdateTool::targets() const {
    return m_targets;
}

void QnMediaServerUpdateTool::setTargets(const QSet<QnUuid> &targets, bool client) {
    m_targets.clear();
    m_enableClientUpdates = client;

    foreach (const QnUuid &id, targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        m_targets.append(server);
    }

    emit targetsChanged(actualTargetIds());
}

QnMediaServerResourceList QnMediaServerUpdateTool::actualTargets() const {
    if (!m_targets.isEmpty())
        return m_targets;

    QnMediaServerResourceList result;
    foreach (const QnMediaServerResourcePtr &server, qnResPool->getResourcesWithFlag(Qn::server).filtered<QnMediaServerResource>()) {
        if (server->getStatus() == Qn::Online)
            result.append(server);
    }

    foreach (const QnMediaServerResourcePtr &server, qnResPool->getAllIncompatibleResources().filtered<QnMediaServerResource>())
    {
        if (server->getModuleInformation().localSystemId == qnGlobalSettings->localSystemId() &&
            server.dynamicCast<QnFakeMediaServerResource>())
        {
            result.append(server);
        }
    }
    return result;
}

QSet<QnUuid> QnMediaServerUpdateTool::actualTargetIds() const {
    QSet<QnUuid> targets;
    foreach (const QnMediaServerResourcePtr &server, actualTargets())
        targets.insert(server->getId());
    return targets;
}

QUrl QnMediaServerUpdateTool::generateUpdatePackageUrl(const QnSoftwareVersion &targetVersion) const {
    QUrlQuery query;

    QString versionSuffix;
    if (targetVersion.isNull()) {
        query.addQueryItem(lit("version"), lit("latest"));
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

    query.addQueryItem(lit("customization"), QnAppInfo::customizationName());

    QUrl url(QnAppInfo::updateGeneratorUrl() + versionSuffix);
    url.setQuery(query);

    return url;
}

void QnMediaServerUpdateTool::checkForUpdates(const QnSoftwareVersion &version, std::function<void(const QnCheckForUpdateResult &result)> func) {
    QnUpdateTarget target(actualTargetIds(), version, !m_enableClientUpdates || qnSettings->isClientUpdateDisabled());
    checkForUpdates(target, func);
}

void QnMediaServerUpdateTool::checkForUpdates(const QString &fileName, std::function<void(const QnCheckForUpdateResult &result)> func) {
    QnUpdateTarget target(actualTargetIds(), fileName, !m_enableClientUpdates || qnSettings->isClientUpdateDisabled());
    checkForUpdates(target, func);
}


void QnMediaServerUpdateTool::startUpdate(const QnSoftwareVersion &version /* = QnSoftwareVersion()*/) {
    QnUpdateTarget target(actualTargetIds(), version, !m_enableClientUpdates || qnSettings->isClientUpdateDisabled());
    startUpdate(target);
}

void QnMediaServerUpdateTool::startUpdate(const QString &fileName) {
    QnUpdateTarget target(actualTargetIds(), fileName, !m_enableClientUpdates || qnSettings->isClientUpdateDisabled());
    startUpdate(target);
}

void QnMediaServerUpdateTool::startOnlineClientUpdate(const QnSoftwareVersion &version) {
    QnUpdateTarget target(QSet<QnUuid>(), version, !m_enableClientUpdates || qnSettings->isClientUpdateDisabled());
    startUpdate(target);
}

bool QnMediaServerUpdateTool::canCancelUpdate() const
{
    if (!m_updateProcess)
        return true;

    if (m_stage == QnFullUpdateStage::Servers)
        return false;

    return true;
}

bool QnMediaServerUpdateTool::cancelUpdate() {
    if (!m_updateProcess)
        return true;

    if (m_stage == QnFullUpdateStage::Servers)
        return false;

    setTargets(QSet<QnUuid>(), defaultEnableClientUpdates);
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
    setTargets(QSet<QnUuid>(), defaultEnableClientUpdates);
}


void QnMediaServerUpdateTool::startUpdate(const QnUpdateTarget& target)
{
    if (m_updateProcess)
        return;

    QSet<QnUuid> incompatibleTargets;
    bool clearTargetsWhenFinished = false;

    if (m_targets.isEmpty())
    {
        clearTargetsWhenFinished = true;
        setTargets(target.targets, defaultEnableClientUpdates);

        for (const auto& id: target.targets)
        {
            const auto server = qnResPool->getIncompatibleResourceById(id)
                .dynamicCast<QnFakeMediaServerResource>();
            if (!server)
                continue;

            const auto realId = server->getOriginalGuid();
            if (realId.isNull())
                continue;

            incompatibleTargets.insert(realId);
            qnDesktopClientMessageProcessor->incompatibleServerWatcher()->keepServer(realId, true);
        }
    }

    m_updateProcess = new QnUpdateProcess(target);
    connect(m_updateProcess, &QnUpdateProcess::stageChanged,
        this, &QnMediaServerUpdateTool::setStage);
    connect(m_updateProcess, &QnUpdateProcess::progressChanged,
        this, &QnMediaServerUpdateTool::setStageProgress);
    connect(m_updateProcess, &QnUpdateProcess::peerStageChanged,
        this, &QnMediaServerUpdateTool::setPeerStage);
    connect(m_updateProcess, &QnUpdateProcess::peerStageProgressChanged,
        this, &QnMediaServerUpdateTool::setPeerStageProgress);
    connect(m_updateProcess, &QnUpdateProcess::updateFinished,
        this, &QnMediaServerUpdateTool::finishUpdate);
    connect(m_updateProcess, &QnUpdateProcess::targetsChanged,
        this, &QnMediaServerUpdateTool::targetsChanged);
    connect(m_updateProcess, &QnUpdateProcess::lowFreeSpaceWarning,
        this, &QnMediaServerUpdateTool::lowFreeSpaceWarning, Qt::BlockingQueuedConnection);

    connect(m_updateProcess, &QThread::finished, this,
        [this, incompatibleTargets, clearTargetsWhenFinished]()
        {
            const auto watcher = qnDesktopClientMessageProcessor->incompatibleServerWatcher();

            m_updateProcess->deleteLater();
            m_updateProcess = nullptr;

            for (const auto& id: incompatibleTargets)
                watcher->keepServer(id, false);

            if (clearTargetsWhenFinished)
                setTargets(QSet<QnUuid>(), defaultEnableClientUpdates);
        });

    m_updateProcess->start();
}

