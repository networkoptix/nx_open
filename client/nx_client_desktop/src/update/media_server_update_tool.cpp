#include "media_server_update_tool.h"

#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#include <QtConcurrent/QtConcurrent>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/incompatible_server_watcher.h>
#include <utils/update/update_utils.h>
#include <update/task/check_update_peer_task.h>
#include <update/low_free_space_warning.h>

#include <client/client_settings.h>
#include <client/desktop_client_message_processor.h>

#include <utils/common/app_info.h>
#include <core/resource/fake_media_server.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

namespace {

    const bool defaultEnableClientUpdates = true;

    QnSoftwareVersion getCurrentVersion(QnResourcePool* resourcePool)
    {
        QnSoftwareVersion minimalVersion = qnStaticCommon->engineVersion();
        const auto allServers = resourcePool->getAllServers(Qn::AnyStatus);
        for(const QnMediaServerResourcePtr &server: allServers)
        {
            if (server->getVersion() < minimalVersion)
                minimalVersion = server->getVersion();
        }
        return minimalVersion;
    }

} // anonymous namespace

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject* parent):
    base_type(parent),
    m_stage(QnFullUpdateStage::Init),
    m_enableClientUpdates(defaultEnableClientUpdates)
{
    auto targetsWatcher = [this](const QnResourcePtr &resource) {
        if (!resource->hasFlags(Qn::server))
            return;

        if (!m_targets.isEmpty())
            return;

        emit targetsChanged(actualTargetIds());
    };
    connect(resourcePool(),  &QnResourcePool::resourceAdded,     this,   targetsWatcher);
    connect(resourcePool(),  &QnResourcePool::resourceChanged,   this,   targetsWatcher);
    connect(resourcePool(),  &QnResourcePool::resourceRemoved,   this,   targetsWatcher);
}

QnMediaServerUpdateTool::~QnMediaServerUpdateTool()
{
    if (m_updateProcess)
    {
        m_updateProcess->stop();
        delete m_updateProcess;
    }

    if (m_checkUpdatesTask)
    {
        m_checkUpdatesTask->cancel();
        delete m_checkUpdatesTask;
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

bool QnMediaServerUpdateTool::isCheckingUpdates() const
{
    return m_checkUpdatesTask;
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

void QnMediaServerUpdateTool::finishUpdate(const QnUpdateResult &result)
{
    setStage(QnFullUpdateStage::Init);

    m_updateProcess->deleteLater();
    m_updateProcess = nullptr;

    /* We must emit signal after m_updateProcess clean, so ::isUpdating() will return false. */
    emit updateFinished(result);
}

QnMediaServerResourceList QnMediaServerUpdateTool::targets() const {
    return m_targets;
}

void QnMediaServerUpdateTool::setTargets(const QSet<QnUuid> &targets, bool client) {
    m_targets.clear();
    m_enableClientUpdates = client;

    foreach (const QnUuid &id, targets) {
        auto server = resourcePool()->getIncompatibleServerById(id, true);
        if (!server)
            continue;

        m_targets.append(server);
    }

    emit targetsChanged(actualTargetIds());
}

QnMediaServerResourceList QnMediaServerUpdateTool::actualTargets() const {
    if (!m_targets.isEmpty())
        return m_targets;

    auto result = resourcePool()->getAllServers(Qn::Online);

    for (const auto& server: resourcePool()->getIncompatibleServers())
    {
        if (helpers::serverBelongsToCurrentSystem(server))
        {
            NX_EXPECT(server.dynamicCast<QnFakeMediaServerResource>());
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

QUrl QnMediaServerUpdateTool::generateUpdatePackageUrl(const QnSoftwareVersion &targetVersion,
    const QString& targetChangeset) const
{
    QUrlQuery query;

    QString versionSuffix; //< TODO: #dklychkov what's that?

    if (targetVersion.isNull())
    {
        query.addQueryItem(lit("version"), lit("latest"));
        query.addQueryItem(lit("current"), getCurrentVersion(resourcePool()).toString());
    }
    else
    {
        const auto key = targetChangeset.isEmpty()
            ? QString::number(targetVersion.build())
            : targetChangeset;

        query.addQueryItem(lit("version"), targetVersion.toString());
        query.addQueryItem(lit("password"), passwordForBuild(key));
    }

    QSet<QnSystemInformation> systemInformationList;
    for (const auto& server: actualTargets())
    {
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

    m_updateProcess->stop();

    return true;
}

bool QnMediaServerUpdateTool::cancelUpdatesCheck()
{
    if (!m_checkUpdatesTask)
        return false;

    m_checkUpdatesTask->cancel();
    delete m_checkUpdatesTask;

    emit updatesCheckCanceled();

    return true;
}

void QnMediaServerUpdateTool::checkForUpdates(
    const QnUpdateTarget& target,
    std::function<void(const QnCheckForUpdateResult& result)> callback)
{
    if (m_checkUpdatesTask)
        return;

    m_checkUpdatesTask = new QnCheckForUpdatesPeerTask(target);

    if (callback)
    {
        connect(m_checkUpdatesTask, &QnCheckForUpdatesPeerTask::checkFinished, this, callback);
    }
    else
    {
        connect(m_checkUpdatesTask, &QnCheckForUpdatesPeerTask::checkFinished,
            this, &QnMediaServerUpdateTool::checkForUpdatesFinished);
    }

    connect(m_checkUpdatesTask, &QnNetworkPeerTask::finished,
        m_checkUpdatesTask, &QObject::deleteLater);

    m_checkUpdatesTask->start();
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
            const auto server = resourcePool()->getIncompatibleServerById(id)
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
    connect(m_updateProcess, &QnUpdateProcess::targetsChanged,
        this, &QnMediaServerUpdateTool::targetsChanged);
    connect(m_updateProcess, &QnUpdateProcess::lowFreeSpaceWarning,
        this, &QnMediaServerUpdateTool::lowFreeSpaceWarning, Qt::BlockingQueuedConnection);

    connect(m_updateProcess, &QnUpdateProcess::updateFinished, this,
        [this, incompatibleTargets, clearTargetsWhenFinished](const QnUpdateResult& result)
        {
            finishUpdate(result);

            const auto watcher = qnDesktopClientMessageProcessor->incompatibleServerWatcher();
            for (const auto& id: incompatibleTargets)
                watcher->keepServer(id, false);

            if (clearTargetsWhenFinished)
                setTargets(QSet<QnUuid>(), defaultEnableClientUpdates);
        });

    m_updateProcess->start();
}

