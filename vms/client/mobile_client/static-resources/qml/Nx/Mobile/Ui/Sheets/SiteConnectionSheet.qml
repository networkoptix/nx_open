// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Mobile.Controls
import Nx.Models

import nx.vms.client.core
import nx.vms.client.mobile

BottomSheet
{
    id: sheet

    property alias authDataModel: authDataModel
    property alias hostsModel: hostsModel

    title: d.systemName && d.systemName.length
        ? d.systemName
        : qsTr("Connect to Site")

    closeAutomatically: !d.connectingNow

    function connectToSite(
        systemName, systemId, localSystemId)
    {
        addressField.text = ""
        userComboBox.editText = ""
        passwordInput.text = ""

        d.resetErrors()

        d.systemName = systemName ?? ""

        hostsModel.systemId = systemId ?? ""
        hostsModel.localSystemId = localSystemId ?? ""

        addressField.text = (hostsModel.rowCount() > 0
            ? NxGlobals.modelData(hostsModel.index(0, 0), "url-internal").toString()
            : NxGlobals.emptyUrl().toString()).trim()

        if (userComboBox.count)
            userComboBox.currentIndex = 0

        passwordInput.resetState(authDataModel.hasSavedCredentials)

        open()

        addressField.visible = !addressField.text.length

        if (!addressField.text.length)
            addressField.forceActiveFocus()
        else if (!userComboBox.editText.length)
            userComboBox.forceActiveFocus()
        else
            passwordInput.forceActiveFocus()
    }

    TextInput
    {
        id: addressField

        backgroundMode: FieldBackground.Mode.Light

        width: parent.width
        labelText: qsTr("Address")

        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData

        enabled: !d.connectingNow
    }

    ComboBox
    {
        id: userComboBox

        backgroundMode: FieldBackground.Mode.Light

        model: authDataModel

        editable: true
        width: parent.width
        popupTitle: qsTr("Log in as")
        labelText: qsTr("Login")

        enabled: !d.connectingNow
        onCurrentIndexChanged: d.resetErrors()
    }

    PasswordInput
    {
        id: passwordInput

        backgroundMode: FieldBackground.Mode.Light

        width: parent.width
        labelText: qsTr("Password")

        enabled: !d.connectingNow

        onAccepted: d.connectToSite()
        onSecretIsCleared: appContext.credentialsHelper.removeSavedAuth(
            hostsModel.localSystemId, d.login)
    }

    ButtonBox
    {
        id: buttonBox

        model: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "connect", type: Button.Type.Brand, text: qsTr("Connect")}]

        onClicked:
            (id) =>
            {
                if (id == "cancel")
                {
                    if (windowContext.sessionManager.hasSession)
                        windowContext.sessionManager.stopSessionByUser()

                    d.connectingNow = false
                    close()
                    return
                }

                if (!d.connectingNow)
                    d.connectToSite()
            }
    }

    Binding
    {
        target: buttonBox.buttonById("connect")
        property: "showBusyIndicator"
        value: d.connectingNow
    }

    Binding
    {
        when: !!hostsModel && !!authDataModel
        target: authDataModel
        property: "localSystemId"
        value: hostsModel.localSystemId
    }

    SystemHostsModel
    {
        id: hostsModel
    }

    AuthenticationDataModel
    {
        id: authDataModel

        currentUser: d.login
        onHasSavedCredentialsChanged: passwordInput.resetState(hasSavedCredentials)
    }

    NxObject
    {
        id: d

        property bool connectingNow: false
        readonly property string login: userComboBox.editText.trim()
        property string systemName

        function resetErrors()
        {
            addressField.errorText = ""
            userComboBox.errorText = ""
            passwordInput.errorText = ""
        }

        function ensureDataEntered()
        {
            d.resetErrors()

            if (!addressField.text.trim().length)
            {
                addressField.visible = true
                addressField.errorText = qsTr("Enter server address")
                addressField.forceActiveFocus()
                return false
            }

            if (!userComboBox.editText.trim().length)
            {
                userComboBox.errorText = qsTr("Login field cannot be empty")
                userComboBox.forceActiveFocus()
                return false
            }

            if (!passwordInput.text.trim())
            {
                passwordInput.errorText = qsTr("Password field cannot be empty")
                passwordInput.forceActiveFocus()
                return false
            }

            return true
        }

        function connectToSite()
        {
            if (!ensureDataEntered())
                return

            d.connectingNow = true

            // Using callLater to avoid problems with animations and event queue
            Qt.callLater(
                ()=>
                {
                    const callback =
                        (errorCode, errorMessage) =>
                        {
                            d.connectingNow = false
                            if (errorCode === SessionManager.SuccessConnectionStatus)
                            {
                                sheet.close()
                                return
                            }

                            switch (errorCode)
                            {
                                case SessionManager.LocalSessionExiredStatus:
                                    appContext.credentialsHelper.removeSavedAuth(
                                        hostsModel.localSystemId, d.login)
                                    // Fallthrough.
                                case SessionManager.UserTemporaryLockedOutConnectionStatus:
                                    // Fallthrough.
                                case SessionManager.UnauthorizedConnectionStatus:
                                    if (errorCode == SessionManager.UnauthorizedConnectionStatus)
                                    {
                                        passwordInput.errorText = errorMessage
                                        passwordInput.forceActiveFocus()
                                    }
                                    else
                                    {
                                        userComboBox.errorText = errorMessage
                                        userComboBox.forceActiveFocus()
                                    }
                                    break

                                default:
                                    addressField.errorText = errorMessage
                                    addressField.forceActiveFocus()
                                    break
                            }
                        }

                    const url = NxGlobals.urlFromUserInput(addressField.text)
                    if (authDataModel.hasSavedCredentials && passwordInput.hasSecretValue)
                    {
                        windowContext.sessionManager.startSessionWithStoredCredentials(
                            url,
                            NxGlobals.uuid(authDataModel.localSystemId),
                            d.login,
                            callback)
                    }
                    else
                    {
                        windowContext.sessionManager.startSession(
                            url,
                            d.login,
                            passwordInput.text,
                            d.systemName,
                            callback)
                    }
                })
        }
    }

    onOpenedChanged:
        windowContext.deprecatedUiController.avoidHandlingConnectionStuff = sheet.opened
    Component.onDestruction:
        windowContext.deprecatedUiController.avoidHandlingConnectionStuff = false
}
