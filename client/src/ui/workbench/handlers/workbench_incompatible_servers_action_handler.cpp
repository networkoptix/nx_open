#include "workbench_incompatible_servers_action_handler.h"

#include <QtWidgets/QMessageBox>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>

#include <utils/connect_to_current_system_tool.h>

QnWorkbenchIncompatibleServersActionHandler::QnWorkbenchIncompatibleServersActionHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_connectToCurrentSystemTool(NULL)
{
    connect(action(Qn::ConnectToCurrentSystem),         SIGNAL(triggered()),    this,   SLOT(at_connectToCurrentSystemAction_triggered()));
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered() {
    QnConnectToCurrentSystemTool *tool = connectToCurrentSystemTool();

    if (tool->isRunning()) {
        QMessageBox::critical(mainWindow(), tr("Error"), tr("Please, wait before the previously requested servers will be added to your system."));
        return;
    }

    QSet<QnId> targets;
    foreach (const QnResourcePtr &resource, menu()->currentParameters(sender()).resources()) {
        if (resource->hasFlags(QnResource::remote_server) && resource->getStatus() == QnResource::Incompatible)
            targets.insert(resource->getId());
    }

    if (targets.isEmpty())
        return;

    tool->connectToCurrentSystem(targets);
}

QnConnectToCurrentSystemTool *QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystemTool() {
    if (!m_connectToCurrentSystemTool)
        m_connectToCurrentSystemTool = new QnConnectToCurrentSystemTool(this);
    return m_connectToCurrentSystemTool;
}
