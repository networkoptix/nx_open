import QtQuick 2.6
import Nx 1.0

Object
{
    Connections
    {
        target: uiController

        onConnectRequested:
        {
            sideNavigation.close()
            connectionManager.connectToServer(url)
            Workflow.openResourcesScreen()
        }

        onDisconnectRequested:
        {
            clearLastUsedConnection()
            sideNavigation.close()
            connectionManager.disconnectFromServer(false)
            Workflow.openSessionsScreen()
        }
        onResourcesScreenRequested: Workflow.openResourcesScreen(connectionManager.systemName)
        onVideoScreenRequested: Workflow.openVideoScreen(resourceId)
    }
}
