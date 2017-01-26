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
            if (liteMode)
                autoLoginEnabled = false

            clearLastUsedConnection()
            sideNavigation.close()
            connectionManager.disconnectFromServer(false)

            if (liteMode)
                Workflow.openLiteClientWelcomeScreen()
            else
                Workflow.openSessionsScreen()
        }
        onResourcesScreenRequested: Workflow.openResourcesScreen(connectionManager.systemName)
        onVideoScreenRequested: Workflow.openVideoScreen(resourceId)
    }
}
