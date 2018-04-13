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
            Workflow.openResourcesScreen("", filterIds)
            connectionManager.connectToServer(url)
        }

        onDisconnectRequested:
        {
            if (liteMode)
                autoLoginEnabled = false

            clearLastUsedConnection()
            sideNavigation.close()
            connectionManager.disconnectFromServer()

            if (liteMode)
                Workflow.openLiteClientWelcomeScreen()
            else
                Workflow.openSessionsScreen()
        }

        onResourcesScreenRequested: Workflow.openResourcesScreen(connectionManager.systemName, filterIds)
        onVideoScreenRequested: Workflow.openVideoScreen(resourceId)
        onLoginToCloudScreenRequested: Workflow.openCloudScreen(user, password, connectOperationId)
    }
}
