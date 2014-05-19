#include "connect_to_current_system_tool.h"

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QObject *parent) :
    QObject(parent)
{
}

void QnConnectToCurrentSystemTool::connectToCurrentSystem(const QSet<QnId> &targets) {

}

bool QnConnectToCurrentSystemTool::isRunning() const {
    return m_running;
}
