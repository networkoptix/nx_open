import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Page
{
    id: customConnectionScreen

    title: systemName ? systemName : qsTr("Connect to Server")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    property string systemName
    property alias address: credentialsEditor.address
    property alias login: credentialsEditor.login
    property alias password: credentialsEditor.password
    property bool saved: false

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
        spacing: 8
        enabled: !d.connecting

        property real availableWidth: width - leftPadding - rightPadding

        SessionCredentialsEditor
        {
            id: credentialsEditor
            width: parent.availableWidth
            onAccepted: customConnectionScreen.connect()
        }

        Item
        {
            width: 1
            height: 8
        }

        LoginButton
        {
            id: connectButton

            width: parent.availableWidth
            showProgress: d.connecting
            onClicked: customConnectionScreen.connect()
        }

        Button
        {
            width: parent.availableWidth
            text: qsTr("Delete")
            visible: saved

            onClicked:
            {
                removeSavedConnection(systemName)
                Workflow.popCurrentScreen()
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
        connectionManager.connectToServer(LoginUtils.makeUrl(address, login, password))
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
        credentialsEditor.focusAddressField()
    }

    function focusLoginField()
    {
        credentialsEditor.focusLoginField()
    }
}
