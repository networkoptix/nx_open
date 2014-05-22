#include "connect_to_current_system_tool.h"

#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <utils/network/global_module_finder.h>
#include <utils/change_system_name_peer_task.h>
#include <utils/media_server_update_tool.h>
#include <utils/common/software_version.h>
#include <ui/dialogs/update_dialog.h>

#include "version.h"

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QObject *parent) :
    QObject(parent),
    m_running(false),
    m_changeSystemNameTask(new QnChangeSystemNamePeerTask(this)),
    m_updateDialog(new QnUpdateDialog())
{
    m_updateDialog->QObject::setParent(this);
    m_updateTool = m_updateDialog->updateTool();

    connect(m_changeSystemNameTask,     &QnNetworkPeerTask::finished,               this,       &QnConnectToCurrentSystemTool::at_changeSystemNameTask_finished);
    connect(m_updateTool,               &QnMediaServerUpdateTool::stateChanged,     this,       &QnConnectToCurrentSystemTool::at_updateTool_stateChanged);
}

void QnConnectToCurrentSystemTool::connectToCurrentSystem(const QSet<QnId> &targets) {
    if (m_running)
        return;

    m_running = true;
    m_targets = targets;
    m_changeSystemNameTask->setSystemName(qnCommon->localSystemName());
    m_changeSystemNameTask->start(targets);
}

bool QnConnectToCurrentSystemTool::isRunning() const {
    return m_running;
}

void QnConnectToCurrentSystemTool::finish(ErrorCode errorCode) {
    m_running = false;
    emit finished(errorCode);
}

void QnConnectToCurrentSystemTool::at_changeSystemNameTask_finished(int errorCode, const QSet<QnId> &failedPeers) {
    Q_UNUSED(failedPeers)

    if (errorCode == 0) {
        m_updateTool->setTargets(m_targets);
        m_updateDialog->show();
        m_prevToolState = QnMediaServerUpdateTool::CheckingForUpdates;
        m_updateTool->checkForUpdates(QnSoftwareVersion(lit(QN_APPLICATION_VERSION)));
    } else {
        finish(SystemNameChangeFailed);
    }
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
            break;
        case QnMediaServerUpdateTool::NoNewerVersion:
            finish(NoError);
            break;
        default:
            finish(UpdateFailed);
        }
    } else if (m_prevToolState == QnMediaServerUpdateTool::DownloadingUpdate) {
        switch (m_updateTool->updateResult()) {
        case QnMediaServerUpdateTool::UpdateSuccessful:
            finish(NoError);
            break;
        default:
            finish(UpdateFailed);
        }
    }
}
