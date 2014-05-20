#include "connect_to_current_system_tool.h"

#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <utils/network/global_module_finder.h>
#include <utils/change_system_name_peer_task.h>

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QObject *parent) :
    QObject(parent),
    m_changeSystemNameTask(new QnChangeSystemNamePeerTask(this))
{
    connect(m_changeSystemNameTask,     &QnNetworkPeerTask::finished,       this,       &QnConnectToCurrentSystemTool::at_changeSystemnameTask_finished);
}

void QnConnectToCurrentSystemTool::connectToCurrentSystem(const QSet<QnId> &targets) {
    if (m_running)
        return;

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

void QnConnectToCurrentSystemTool::at_changeSystemnameTask_finished(int errorCode, const QSet<QnId> &failedPeers) {
    if (errorCode == 0) {
        emit finished(NoError);
    }
}
