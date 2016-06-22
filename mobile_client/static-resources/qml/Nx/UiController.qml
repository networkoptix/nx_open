import QtQuick 2.6
import Nx 1.0

Object
{
    Connections
    {
        target: uiController

        onResourcesScreenRequested: Workflow.openResourcesScreen(connectionManager.systemName)
        onVideoScreenRequested: Workflow.openVideoScreen(resourceId)
    }
}
