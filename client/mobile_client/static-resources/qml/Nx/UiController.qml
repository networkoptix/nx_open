import QtQuick 2.6
import Nx 1.0

Object
{
    Connections
    {
        target: uiController

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
