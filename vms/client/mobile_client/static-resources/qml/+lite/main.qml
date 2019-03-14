import QtQuick 2.0
import QtQuick.Window 2.2
import QtQuick.Controls 2.4
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

ApplicationWindow
{
    id: mainWindow

    visible: true
    color: ColorTheme.windowBackground

    QtObject
    {
        // Dummy object. It is here to avoid numerous checks for liteMode.
        id: sideNavigation
        property bool enabled: false
        property bool opened: false
        function open() {}
        function close() {}
    }

    StackView
    {
        id: stackView
        anchors.fill: parent

        function restoreActiveFocus()
        {
            if (activeFocusItem == Window.contentItem)
                Workflow.focusCurrentScreen()
        }

        Keys.onPressed:
        {
            if (Utils.keyIsBack(event.key))
                uiController.disconnectFromSystem()
        }
    }

    UiController {}

    Component.onCompleted:
    {
        if (autoLoginEnabled)
        {
            var url = getInitialUrl()
            var systemName = ""

            if (url.isEmpty())
            {
                url = getLastUsedUrl()
                systemName = getLastUsedSystemName()
            }

            if (!url.isEmpty())
            {
                Workflow.openResourcesScreen(systemName)
                connectionManager.connectToServer(url)
                return
            }
        }

        Workflow.openLiteClientWelcomeScreen()
    }

    Component.onDestruction:
    {
        connectionManager.disconnectFromServer()
    }

    onActiveFocusItemChanged: stackView.restoreActiveFocus()
}
