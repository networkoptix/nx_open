import QtQuick 2.0
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Page
{
    id: resourcesScreen
    objectName: "resourcesScreen"

    leftButtonIcon: "/images/menu.png"
    onLeftButtonClicked: sideNavigation.open()

    QtObject
    {
        id: d
    }

    Connections
    {
        target: connectionManager

        onConnectionStateChanged:
        {
            if (connectionManager.connectionState === QnConnectionManager.Connected)
            {
                savedSessionsModel.updateSession(
                            currentSessionId,
                            connectionManager.currentUrl,
                            connectionManager.systemName,
                            true)
                loginSessionManager.lastUsedSessionId = currentSessionId
            }
        }

        onConnectionFailed:
        {
            var sessionId = currentSessionId
            currentSessionId = ""
            Workflow.openFailedSessionScreen(
                        sessionId,
                        title,
                        connectionManager.currentHost,
                        connectionManager.currentPort,
                        connectionManager.currentLogin,
                        connectionManager.currentPassword,
                        status,
                        infoParameter)
        }
    }
}
