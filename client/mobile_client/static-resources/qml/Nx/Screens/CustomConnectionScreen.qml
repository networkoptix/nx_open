import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Dialogs 1.0
import Nx.Models 1.0
import com.networkoptix.qml 1.0

Page
{
    id: customConnectionScreen

    title: systemName ? systemName : qsTr("Connect to Server")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    property alias systemId: systemHostsModel.systemId
    property alias localSystemId: authenticationDataModel.systemId
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

            hostsModel: SystemHostsModel
            {
                id: systemHostsModel
            }

            authenticationDataModel: AuthenticationDataModel
            {
                id: authenticationDataModel
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
                var title
                var message

                var lastCredentials = authenticationDataAccessor.count <= 1

                if (!lastCredentials)
                {
                    title = qsTr("Delete login \"%1\"?").arg(credentialsEditor.login)
                    message = qsTr(
                        "Server addresses and other logins will remain saved. "
                            + "To delete all connection information "
                            + "you should delete all saved logins.")
                }
                else
                {
                    title = qsTr("Delete connection?")
                }

                var dialog = Workflow.openStandardDialog(
                    title, message,
                    [
                        { "id": "DELETE", "text": qsTr("Delete") },
                        "CANCEL"
                    ])

                dialog.buttonClicked.connect(
                    function(buttonId)
                    {
                        if (buttonId !== "DELETE")
                            return

                        removeSavedConnection(localSystemId, credentialsEditor.login)

                        if (lastCredentials)
                        {
                            Workflow.popCurrentScreen()
                            return
                        }

                        var credentials = authenticationDataModel.defaultCredentials
                        credentialsEditor.login = credentials.user
                        credentialsEditor.password = credentials.password
                    })
            }
        }
    }

    ModelDataAccessor
    {
        id: authenticationDataAccessor
        model: authenticationDataModel
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

    function checkConnectionFields()
    {
        hideWarning()

        if (credentialsEditor.address.trim().length == 0)
        {
            credentialsEditor.addressErrorText = qsTr("Enter server address")
            credentialsEditor.displayAddressError = true
            return false
        }
        else if ((credentialsEditor.login.trim().length == 0)
            || (credentialsEditor.password.trim().length == 0))
        {
            credentialsEditor.credentialsErrorText = qsTr("All fields must be filled")
            credentialsEditor.displayUserCredentialsError = true
            return false
        }
        return true
    }

    function connect()
    {
        if (!checkConnectionFields())
            return

        connectButton.forceActiveFocus()
        d.connecting = true
        connectionManager.connectByUserInput(address, login, password)
    }

    function showWarning(status, info)
    {
        if (status == QnConnectionManager.UnauthorizedConnectionResult)
        {
            credentialsEditor.credentialsErrorText =
                LoginUtils.connectionErrorText(QnConnectionManager.UnauthorizedConnectionResult)
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
