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

    function connectToServerByUrl(url)
    {
        console.log(">>>>>>>>>>>>>>>>> CONNECT BY URL")
        connectionManager.connectToServer(url,
            function(state, result, extraInfo)
            {
                d.connectionCallback(state, result, extraInfo, true)
            })

    }

    function connectToServerByParams(address, user, password, cloudConnection)
    {
        console.log(">>>>>>>>>>>>>>>>> CONNECT BY PARAMS")
        connectionManager.connectToServer(address, user, password, cloudConnection,
            function(state, result, extraInfo)
            {
                d.connectionCallback(state, result, extraInfo)
            })
    }

    function connectByUserInput(
        url, user, password,
        failedCallback,
        connectedCallback)
    {
        console.log(">>>>>>>>>>>>>>>>> CONNECT BY USER INPUT")
        connectionManager.connectByUserInput(url, user, password,
            function(state, result, extraInfo)
            {
                d.connectionCallback(
                    state, result, extraInfo, false, failedCallback, connectedCallback)
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

        function managerAvailable() //< We need it to prevent "undefined variable" warning at exit.
        {
            return typeof(connectionManager) !== 'undefined'
        }

        function connectionCallback(
            state, result, extraInfo, showPreloader, failedCallback, connectedCallback)
        {
            switch (state)
            {
                case QnConnectionManager.Disconnected: //< Connection failed.
                    console.log(">>> DISCONNECTED")
                    if (stackView.depth > 1)
                        stackView.pop(stackView.currentItem)
                    else
                        Workflow.openSessionsScreenWithWarning(connectionManager.systemName)

                    if (failedCallback)
                        failedCallback(result, extraInfo)
                    break

                case QnConnectionManager.Connecting:
                    console.log(">>> CONNECTING")
                    if (showPreloader || !stackView.depth)
                        showConnectionPreloader()
                    break

                case QnConnectionManager.Connected: //< Connected and loading resources now.
                    console.log(">>> CONNECTED")
                    showConnectionPreloader()
                    if (connectedCallback)
                        connectedCallback()
                    break
            }
        }

        function showConnectionPreloader()
        {
            if (!stackView.currentItem || stackView.currentItem.objectName !== "resourcesScreen")
                Workflow.openResourcesScreen(connectionManager.systemName)
        }
    }
}
