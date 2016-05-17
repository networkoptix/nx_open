import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Page
{
    id: customConnectionScreen

    title: qsTr("Connect to Server")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    property alias host: credentialsEditor.host
    property alias port: credentialsEditor.port
    property alias login: credentialsEditor.login
    property alias password: credentialsEditor.password
    property string sessionId: ""

    QtObject
    {
        id: d
        property bool connecting: false
    }

    Column
    {
        width: parent.width
        leftPadding: 16
        rightPadding: 16
        spacing: 24
        enabled: !d.connecting

        property real contentWidth: width - leftPadding - rightPadding

        SessionCredentialsEditor
        {
            id: credentialsEditor
            width: parent.contentWidth
            onAccepted: customConnectionScreen.connect()
        }

        Item
        {
            width: parent.contentWidth
            height: connectButton.height

            Button
            {
                id: connectButton

                width: parent.width
                text: qsTr("Connect")
                color: ColorTheme.brand_main
                textColor: ColorTheme.brand_contrast
                onClicked: customConnectionScreen.connect()
                opacity: d.connecting ? 0.0 : 1.0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }

            ThreeDotBusyIndicator
            {
                anchors.centerIn: parent
                opacity: 1.0 - connectButton.opacity
            }
        }
    }

    Connections
    {
        target: d.connecting ? connectionManager : null

        onConnectionStateChanged:
        {
            if (connectionManager.connectionState === QnConnectionManager.Connected)
            {
                currentSessionId = savedSessionsModel.updateSession(
                        sessionId, host, port, login, password, connectionManager.systemName, true)
                Workflow.openResourcesScreen(connectionManager.systemName)
            }
        }

        onConnectionFailed:
        {
            d.connecting = false
            showWarning(status, infoParameter)
        }
    }

    function connect()
    {
        hideWarning()
        connectButton.forceActiveFocus()
        d.connecting = true
        connectionManager.connectToServer(LoginUtils.makeUrl(host, port, login, password))
    }

    function showWarning(status, info)
    {
        warningText = LoginUtils.connectionErrorText(status, info)
        warningVisible = true

        if (status === QnConnectionManager.Unauthorized)
            credentialsEditor.displayUserCredentialsError = true
        else
            credentialsEditor.displayAddressError = true

        if (status === QnConnectionManager.InvalidVersion)
            Workflow.openOldClientDownloadSuggestion()
    }

    function hideWarning()
    {
        warningVisible = false
        credentialsEditor.displayUserCredentialsError = false
        credentialsEditor.displayAddressError = false
    }

    function focusHostField()
    {
        credentialsEditor.focusHostField()
    }

    function focusLoginField()
    {
        credentialsEditor.focusLoginField()
    }
}
