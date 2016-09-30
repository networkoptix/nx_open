#include "connect_to_current_system_tool.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>

#include <update/task/configure_peer_task.h>
#include <update/task/wait_compatible_servers_peer_task.h>
#include <update/media_server_update_tool.h>

#include <utils/common/software_version.h>


#include <utils/common/app_info.h>

namespace
{
    static const int configureProgress = 25;
    static const int waitProgress = 50;
    static const int completeProgress = 100;

    enum UpdateToolState
    {
        CheckingForUpdates,
        Updating
    };
}

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_currentTask(0),
    m_updateTool(0),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnConnectToCurrentSystemTool>(this))
{
}

QnConnectToCurrentSystemTool::~QnConnectToCurrentSystemTool() {}

bool QnConnectToCurrentSystemTool::tryClose(bool force)
{
    Q_UNUSED(force)
    cancel();
    return true;
}

void QnConnectToCurrentSystemTool::forcedUpdate()
{
}

void QnConnectToCurrentSystemTool::start(const QSet<QnUuid> &targets, const QString &password)
{
    if (targets.isEmpty())
    {
        finish(NoError);
        return;
    }

    m_targets = targets;
    m_adminPassword = password;
    m_restartTargets.clear();
    m_updateTargets.clear();
    m_waitTargets.clear();

    for (const QnUuid &id: m_targets)
    {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            m_targets.remove(id);
    }

    emit progressChanged(0);
    emit stateChanged(tr("Configuring Server(s)"));

    QnConfigurePeerTask *task = new QnConfigurePeerTask(this);
    task->setAdminPassword(m_adminPassword);
    m_currentTask = task;
    connect(task, &QnNetworkPeerTask::finished, this, &QnConnectToCurrentSystemTool::at_configureTask_finished);

    task->start(m_targets);
}

QSet<QnUuid> QnConnectToCurrentSystemTool::targets() const
{
    return m_targets;
}

QString QnConnectToCurrentSystemTool::adminPassword() const
{
    return m_adminPassword;
}

void QnConnectToCurrentSystemTool::cancel()
{
    if (m_currentTask)
        m_currentTask->cancel();

    if (m_updateTool)
        m_updateTool->cancelUpdate();

    if (m_currentTask || m_updateTool)
        emit finished(Canceled);
}

void QnConnectToCurrentSystemTool::finish(ErrorCode errorCode)
{
    emit finished(errorCode);
}

void QnConnectToCurrentSystemTool::waitPeers()
{
    QnWaitCompatibleServersPeerTask *task = new QnWaitCompatibleServersPeerTask(this);
    m_currentTask = task;

    emit progressChanged(configureProgress);

    connect(task, &QnNetworkPeerTask::finished, this, &QnConnectToCurrentSystemTool::at_waitTask_finished);
    task->start(QSet<QnUuid>::fromList(m_waitTargets.values()));
}

void QnConnectToCurrentSystemTool::updatePeers()
{
    if (m_updateTargets.isEmpty())
    {
        finish(NoError);
        return;
    }

    emit stateChanged(tr("Updating Server(s)"));
    emit progressChanged(waitProgress);

    m_updateTool = new QnMediaServerUpdateTool(this);
    connect(m_updateTool,   &QnMediaServerUpdateTool::updateFinished,           this,   &QnConnectToCurrentSystemTool::at_updateTool_finished);
    connect(m_updateTool,   &QnMediaServerUpdateTool::stageProgressChanged,     this,   &QnConnectToCurrentSystemTool::at_updateTool_stageProgressChanged);

    m_updateTool->setTargets(m_updateTargets);
    QnSoftwareVersion currentVersion(qnCommon->engineVersion().major(), qnCommon->engineVersion().minor());
    m_updateTool->startUpdate(currentVersion);
}

void QnConnectToCurrentSystemTool::at_configureTask_finished(int errorCode, const QSet<QnUuid> &failedPeers)
{
    m_currentTask = 0;

    if (errorCode != 0)
    {
        if (errorCode == QnConfigurePeerTask::AuthentificationFailed)
            finish(AuthentificationFailed);
        else
            finish(ConfigurationFailed);
        return;
    }

    for (const QnUuid &id: m_targets - failedPeers)
    {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (server->getModuleInformation().protoVersion != QnAppInfo::ec2ProtoVersion())
        {
            m_updateTargets.insert(server->getId());
        }
        else
        {
            QnUuid originalId = server->getOriginalGuid();
            if (!originalId.isNull())
                m_waitTargets.insert(server->getId(), originalId);
        }
    }

    waitPeers();
}

void QnConnectToCurrentSystemTool::at_waitTask_finished(int errorCode)
{
    m_currentTask = 0;

    if (errorCode != 0)
    {
        finish(ConfigurationFailed);
        return;
    }

    updatePeers();
}

void QnConnectToCurrentSystemTool::at_updateTool_finished(const QnUpdateResult &result)
{
    m_updateTool = 0;

    if (result.result == QnUpdateResult::Successful)
    {
        emit progressChanged(completeProgress);
        finish(NoError);
    }
    else
    {
        finish(UpdateFailed);
    }
}

void QnConnectToCurrentSystemTool::at_updateTool_stageProgressChanged(QnFullUpdateStage stage, int progress)
{
    int updateProgress = (static_cast<int>(stage) * 100 + progress) / static_cast<int>(QnFullUpdateStage::Count);
    emit progressChanged(waitProgress + updateProgress * (100 - waitProgress) / 100);
}
