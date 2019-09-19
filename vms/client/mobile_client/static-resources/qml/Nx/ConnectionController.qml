pragma Singleton

import QtQuick 2.11

import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Object
{
    id: controller

    readonly property bool online: d.online
    readonly property alias ready: d.readyState
    readonly property alias disconnected: d.disconnectedState
    readonly property bool connecting: d.connectingState || (!d.online && d.connectedState)
    readonly property alias connected: d.connectedState
    readonly property alias reconnecting: d.reconnecting

    property StackView stackView

    function connectToServerByUrl(url, forcedSystemName)
    {
        d.showConnectionPreloader(forcedSystemName)
        connectionManager.connectToServer(url,
            function(state, result, extraInfo)
            {
                d.connectionCallback(state, result, extraInfo, forcedSystemName)
            })
    }

    function connectToServerByParams(address, user, password, forcedSystemName, cloudConnection)
    {
        d.showConnectionPreloader(forcedSystemName)
        connectionManager.connectToServer(address, user, password, cloudConnection,
            function(state, result, extraInfo)
            {
                d.connectionCallback(state, result, extraInfo, forcedSystemName)
            })
    }

    function connectByUserInput(
        url, user, password, forcedSystemName,
        failedCallback,
        connectedCallback)
    {
        connectionManager.connectByUserInput(url, user, password,
            function(state, result, extraInfo)
            {
                d.connectionCallback(
                    state, result, extraInfo, forcedSystemName, failedCallback, connectedCallback)
            })
    }

    Object
    {
        id: d

        readonly property int state: managerAvailable()
            ? connectionManager.connectionState
            : QnConnectionManager.Disconnected
        readonly property bool disconnectedState: state == QnConnectionManager.Disconnected
        readonly property bool connectingState: state == QnConnectionManager.Connecting
        readonly property bool connectedState: state == QnConnectionManager.Connected
        readonly property bool readyState: state == QnConnectionManager.Ready
        readonly property bool online: managerAvailable() ? connectionManager.online : false
        readonly property bool reconnecting: managerAvailable()
            ? connectionManager.restoringConnection
            : false

        function currentScreenName()
        {
            var currentItem = stackView ? stackView.currentItem : undefined
            return currentItem ? currentItem.objectName : ""
        }

        function managerAvailable() //< We need it to prevent "undefined variable" warning at exit.
        {
            return typeof(connectionManager) !== 'undefined'
        }

        function connectionCallback(
            state, result, extraInfo, forcedSystemName, failedCallback, connectedCallback)
        {
            switch (state)
            {
                case QnConnectionManager.Disconnected: //< Connection failed.
                    if (currentScreenName() != "sessionsScreen")
                    {
                        if (!failedCallback || stackView.depth <= 1)
                        {
                            Workflow.openSessionsScreenWithWarning(
                                connectionManager.systemName,
                                LoginUtils.connectionErrorText(result))
                        }
                        else
                        {
                            stackView.pop(stackView.currentItem)
                        }
                    }

                    if (failedCallback)
                        failedCallback(result, extraInfo)
                    break

                case QnConnectionManager.Connected: //< Connected and loading resources now.
                    showConnectionPreloader(forcedSystemName)
                    if (connectedCallback)
                        connectedCallback()
                    break
            }
        }

        function showConnectionPreloader(forcedSystemName)
        {
            if (currentScreenName() === "resourcesScreen")
                return

            Workflow.openResourcesScreen(forcedSystemName
                ? forcedSystemName
                : connectionManager.systemName)
        }
    }
}
