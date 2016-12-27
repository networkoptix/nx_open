import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Page
{
    id: customConnectionScreen

    title: systemName ? systemName : qsTr("Connect to System")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    property alias systemId: systemHostsModel.systemId
    property alias localSystemId: recentLocalConnectionsModel.systemId
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
        leftPadding: 8
        rightPadding: 8
        spacing: 8
        enabled: !d.connecting

        property real availableWidth: width - leftPadding - rightPadding

        SessionCredentialsEditor
        {
            id: credentialsEditor

            hostsModel: QnSystemHostsModel
            {
                id: systemHostsModel
            }

            recentLocalConnectionsModel: QnRecentLocalConnectionsModel
            {
                id: recentLocalConnectionsModel
            }

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
                Workflow.openResourcesScreen(connectionManager.systemName)
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
        connectionManager.connectByUserInput(address, login, password)
    }

    function showWarning(status, info)
    {
        if (status == QnConnectionManager.UnauthorizedConnectionResult)
        {
            credentialsEditor.displayUserCredentialsError = true
        }
        else
        {
            credentialsEditor.addressErrorText = LoginUtils.connectionErrorText(status, info)
            credentialsEditor.displayAddressError = true
        }

        if (status == QnConnectionManager.IncompatibleVersionConnectionResult)
            Workflow.openOldClientDownloadSuggestion()
    }

    function hideWarning()
    {
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
