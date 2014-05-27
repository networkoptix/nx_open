#include "connect_to_current_system_tool.h"

#include <QtWidgets/QMessageBox>

#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <utils/network/global_module_finder.h>
#include <utils/change_system_name_peer_task.h>
#include <utils/restart_peer_task.h>
#include <utils/media_server_update_tool.h>
#include <utils/common/software_version.h>
#include <ui/dialogs/update_dialog.h>

#include "version.h"

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QObject *parent) :
    QObject(parent),
    m_running(false),
    m_changeSystemNameTask(new QnChangeSystemNamePeerTask(this)),
    m_restartPeerTask(new QnRestartPeerTask(this)),
    m_updateDialog(new QnUpdateDialog())
{
    m_updateTool = m_updateDialog->updateTool();

    connect(m_changeSystemNameTask,     &QnNetworkPeerTask::finished,               this,       &QnConnectToCurrentSystemTool::at_changeSystemNameTask_finished);
    connect(m_restartPeerTask,          &QnNetworkPeerTask::finished,               this,       &QnConnectToCurrentSystemTool::at_restartPeerTask_finished);
    // queued connection is used to be sure that we'll get this signal AFTER it will be handled by update dialog
    connect(m_updateTool,               &QnMediaServerUpdateTool::stateChanged,     this,       &QnConnectToCurrentSystemTool::at_updateTool_stateChanged, Qt::QueuedConnection);
}

QnConnectToCurrentSystemTool::~QnConnectToCurrentSystemTool() {}

void QnConnectToCurrentSystemTool::connectToCurrentSystem(const QSet<QnId> &targets) {
    if (m_running)
        return;

    m_running = true;
    m_targets = targets;
    m_restartTargets.clear();
    m_updateTargets.clear();
    changeSystemName();
}

bool QnConnectToCurrentSystemTool::isRunning() const {
    return m_running;
}

void QnConnectToCurrentSystemTool::finish(ErrorCode errorCode) {
    m_running = false;
    m_updateDialog->hide();
    emit finished(errorCode);
}

void QnConnectToCurrentSystemTool::changeSystemName() {
    if (m_targets.isEmpty()) {
        finish(NoError);
        return;
    }

    m_changeSystemNameTask->setSystemName(qnCommon->localSystemName());
    m_changeSystemNameTask->start(m_targets);
}

void QnConnectToCurrentSystemTool::updatePeers() {
    if (m_updateTargets.isEmpty()) {
        restartPeers();
        return;
    }

    m_updateFailed = false;
    m_updateDialog->setTargets(m_updateTargets);
    m_updateDialog->show();
    m_prevToolState = QnMediaServerUpdateTool::CheckingForUpdates;
    m_updateTool->checkForUpdates(qnCommon->engineVersion());
}

void QnConnectToCurrentSystemTool::restartPeers() {
    if (m_restartAllPeers)
        m_restartTargets = m_targets;

    if (m_restartTargets.isEmpty()) {
        finish(NoError);
        return;
    }

    m_restartPeerTask->start(m_restartTargets);
}

void QnConnectToCurrentSystemTool::at_changeSystemNameTask_finished(int errorCode, const QSet<QnId> &failedPeers) {
    if (errorCode != 0) {
        finish(SystemNameChangeFailed);
        return;
    }

    foreach (const QnId &id, m_targets - failedPeers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (server->getVersion() != qnCommon->engineVersion())
            m_updateTargets.insert(id);
        else
            m_restartTargets.insert(id);
    }

    updatePeers();
}

void QnConnectToCurrentSystemTool::at_updateTool_stateChanged(int state) {
    if (state != QnMediaServerUpdateTool::Idle)
        return;

    // decide what to do next.
    // Previous state is not actual tool previous state! It's set manually in code.
    if (m_prevToolState == QnMediaServerUpdateTool::CheckingForUpdates) {
        switch (m_updateTool->updateCheckResult()) {
        case QnMediaServerUpdateTool::UpdateFound:
            m_prevToolState = QnMediaServerUpdateTool::DownloadingUpdate;
            m_updateTool->updateServers();
            return;
        case QnMediaServerUpdateTool::NoNewerVersion:
            m_restartAllPeers = true;
            break;
        default:
            m_restartAllPeers = true;
            m_updateFailed = true;
            break;
        }
    } else if (m_prevToolState == QnMediaServerUpdateTool::DownloadingUpdate) {
        switch (m_updateTool->updateResult()) {
        case QnMediaServerUpdateTool::Cancelled:
        case QnMediaServerUpdateTool::LockFailed:
        case QnMediaServerUpdateTool::DownloadingFailed:
        case QnMediaServerUpdateTool::UploadingFailed:
            m_restartAllPeers = true;
            break;
        default:
            break;
        }

        m_updateFailed = (m_updateTool->updateResult() != QnMediaServerUpdateTool::UpdateSuccessful);
    }

    m_updateDialog->hide();
    restartPeers();
}

void QnConnectToCurrentSystemTool::at_restartPeerTask_finished(int errorCode) {
    if (errorCode != 0) {
        finish(RestartFailed);
        return;
    }

    finish(m_updateFailed ? UpdateFailed : NoError);
}
