// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Dialogs
import Nx.Models
import Nx.Ui

import nx.vms.client.mobile

Page
{
    id: customConnectionScreen
    objectName: "customConnectionScreen"

    title: systemName ? systemName : qsTr("Connect to Server")
    onLeftButtonClicked: cancelScreen();

    property alias systemId: systemHostsModel.systemId
    property alias localSystemId: authenticationDataModel.localSystemId
    property string systemName
    property alias address: credentialsEditor.address
    property alias login: credentialsEditor.login
    property alias passwordErrorText: credentialsEditor.passwordErrorText
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

                authDataModel: AuthenticationDataModel
                {
                    id: authenticationDataModel

                    currentUser: credentialsEditor.login.trim()
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

                            credentialsEditor.login =
                                authenticationDataModel.defaultCredentials.user
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
            credentialsEditor.loginErrorText = qsTr("Login field cannot be empty")
            credentialsEditor.displayLoginError = true
            return false
        }
        else if (credentialsEditor.password.trim().length === 0
            && !credentialsEditor.isPasswordSaved)
        {
            credentialsEditor.passwordErrorText = qsTr("Password field cannot be empty")
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
        CoreUtils.executeDelayed(
            function()
            {
                const callback =
                    (connectionStatus, errorMessage) =>
                    {
                        if (connectionStatus === SessionManager.SuccessConnectionStatus)
                            finishOperation(true)
                        else
                            handleConnectionFailed(connectionStatus, errorMessage)
                    }

                const url = NxGlobals.urlFromUserInput(address)
                if (credentialsEditor.isPasswordSaved)
                {
                    sessionManager.startSessionWithStoredCredentials(
                        url, NxGlobals.uuid(localSystemId), login, callback)
                }
                else
                {
                    sessionManager.startSession(url, login, password, systemName, callback)
                }
            }, kAnimationDelayMs, this)
    }

    function handleConnectionFailed(errorCode, errorMessage)
    {
        d.connecting = false

        switch (errorCode)
        {
            case SessionManager.LocalSessionExiredStatus:
                removeSavedAuth(customConnectionScreen.localSystemId, credentialsEditor.login)
                // Fallthrough.
            case SessionManager.UserTemporaryLockedOutConnectionStatus:
                // Fallthrough.
            case SessionManager.UnauthorizedConnectionStatus:
                const unauthorized = errorCode == SessionManager.UnauthorizedConnectionStatus
                credentialsEditor.passwordErrorText = errorMessage
                credentialsEditor.displayLoginError = unauthorized
                credentialsEditor.displayPasswordError = unauthorized
                focusCredentialsField()
                break

            default:
                credentialsEditor.addressErrorText = errorMessage
                credentialsEditor.displayAddressError = true
                break
        }
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
