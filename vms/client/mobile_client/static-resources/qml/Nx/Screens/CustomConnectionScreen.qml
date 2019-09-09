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
    objectName: "customConnectionScreen"

    title: systemName ? systemName : qsTr("Connect to Server")
    onLeftButtonClicked: cancelScreen();

    property alias systemId: systemHostsModel.systemId
    property alias localSystemId: authenticationDataModel.systemId
    property string systemName
    property alias address: credentialsEditor.address
    property alias login: credentialsEditor.login
    property alias password: credentialsEditor.password
    property bool saved: false
    property string operationId

    QtObject
    {
        id: d
        property bool connecting: false
    }

    Flickable
    {
        anchors.fill: parent
        contentWidth: contentColumn.width
        contentHeight: contentColumn.height
        bottomMargin: 16

        Column
        {
            id: contentColumn

            width: customConnectionScreen.width
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
                    localSystemId: customConnectionScreen.localSystemId
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
                        title = qsTr("Delete login \"%1\"?", "%1 is a user name").arg(credentialsEditor.login)
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

                            removeSavedConnection(systemId, localSystemId, credentialsEditor.login)

                            if (lastCredentials)
                            {
                                cancelScreen()
                                return
                            }

                            var credentials = authenticationDataModel.defaultCredentials
                            credentialsEditor.login = credentials.user
                            credentialsEditor.password = credentials.password
                        })
                }
            }
        }
    }

    ModelDataAccessor
    {
        id: authenticationDataAccessor
        model: authenticationDataModel
    }

    function checkConnectionFields()
    {
        hideWarning()

        if (credentialsEditor.address.trim().length === 0)
        {
            credentialsEditor.addressErrorText = qsTr("Enter server address")
            credentialsEditor.displayAddressError = true
            return false
        }
        else if (credentialsEditor.login.trim().length === 0)
        {
            credentialsEditor.loginErrorText = qsTr("Login cannot be empty")
            credentialsEditor.displayLoginError = true
            return false
        }
        else if (credentialsEditor.password.trim().length === 0)
        {
            credentialsEditor.passwordErrorText = qsTr("Password cannot be empty")
            credentialsEditor.displayPasswordError = true
            return false
        }
        return true
    }

    function connect()
    {
        if (!checkConnectionFields())
            return

        d.connecting = true;
        connectButton.forceActiveFocus()

        // We need this execute delayed to make sure connection animation is started if server is
        // located locally.
        var kAnimationDelayMs = 50
        Utils.executeDelayed(
            function()
            {
                ConnectionController.connectByUserInput(
                    address, login, password, systemName,
                    handleConnectionFailed,
                    handleConnected)
            }, kAnimationDelayMs, this)
    }

    function handleConnected()
    {
        finishOperation(true)
    }

    function handleConnectionFailed(result, extraInfo)
    {
        d.connecting = false

        var lockedOut = result == QnConnectionManager.UserTemporaryLockedOut
        if (lockedOut || result === QnConnectionManager.UnauthorizedConnectionResult)
        {
            credentialsEditor.passwordErrorText = LoginUtils.connectionErrorText(result)
            credentialsEditor.displayLoginError = !lockedOut
            credentialsEditor.displayPasswordError = !lockedOut
        }
        else
        {
            credentialsEditor.addressErrorText = LoginUtils.connectionErrorText(result, extraInfo)
            credentialsEditor.displayAddressError = true
        }

        if (result === QnConnectionManager.IncompatibleVersionConnectionResult)
            Workflow.openOldClientDownloadSuggestion()
    }

    function hideWarning()
    {
        credentialsEditor.displayLoginError = false
        credentialsEditor.displayPasswordError = false
        credentialsEditor.displayAddressError = false
        credentialsEditor.addressErrorText = ""
        credentialsEditor.loginErrorText = ""
        credentialsEditor.passwordErrorText = ""
    }

    function focusHostField()
    {
        credentialsEditor.focusAddressField()
    }

    function focusCredentialsField()
    {
        credentialsEditor.focusCredentialsField()
    }

    function cancelScreen()
    {
        Workflow.popCurrentScreen()
        finishOperation(false)
    }

    function finishOperation(success)
    {
        if (!operationId.length)
            return;

        operationManager.finishOperation(operationId, false)
        operationId = ""
    }
}
