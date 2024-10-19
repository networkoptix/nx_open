// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Nx.Core
import Nx.Controls
import Nx.Dialogs
import Nx.Models

import "private"

Pane
{
    id: credentialsEditor

    implicitHeight: content.height
    implicitWidth: parent ? parent.width : 400
    background: null

    property alias hostsModel: hostSelectionDialog.model
    property alias authDataModel: userSelectionDialog.model
    property alias address: addressField.text
    property alias login: loginField.text
    property alias password: passwordField.text

    property bool displayAddressError: false
    property bool displayLoginError: false
    property bool displayPasswordError: false
    property alias addressErrorText: addressErrorPanel.text
    property alias loginErrorText: loginErrorPanel.text
    property alias passwordErrorText: passwordErrorPanel.text

    readonly property bool isPasswordSaved: authDataModel.hasSavedCredentials

    signal accepted()
    signal changed()

    Column
    {
        id: content
        width: parent.width

        TextField
        {
            id: addressField

            width: parent.width
            placeholderText: qsTr("Host : Port")
            showError: displayAddressError
            onTextChanged: credentialsEditor.changed()
            selectionAllowed: false
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            activeFocusOnTab: true
            onAccepted: nextItemInFocusChain(true).forceActiveFocus()
            rightPadding: chooseHostButton.visible ? chooseHostButton.width : 8

            IconButton
            {
                id: chooseHostButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                icon.source: lp("/images/expand.png")
                onClicked: hostSelectionDialog.open()
                visible: hostsModel.count > 1
            }

            onActiveFocusChanged:
            {
                if (activeFocus)
                    displayAddressError = false
            }
        }

        FieldWarning
        {
            id: addressErrorPanel
            width: parent.width
            opened: text.length
        }

        Item
        {
            width: 1
            height: 24
        }

        TextField
        {
            id: loginField

            width: parent.width
            placeholderText: qsTr("Login")
            showError: displayLoginError
            onTextChanged: credentialsEditor.changed()
            selectionAllowed: false
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
            activeFocusOnTab: true
            rightPadding: chooseUserButton.visible ? chooseUserButton.width : 8

            IconButton
            {
                id: chooseUserButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                icon.source: lp("/images/expand.png")
                onClicked: userSelectionDialog.open()
                visible: authenticationDataAccessor.count > 1
            }

            onAccepted: nextItemInFocusChain(true).forceActiveFocus()
            onActiveFocusChanged:
            {
                if (activeFocus)
                    displayLoginError = false
            }
        }
        FieldWarning
        {
            id: loginErrorPanel
            width: parent.width
            opened: text.length
        }

        Item
        {
            width: parent.width
            height: passwordField.height

            TextField
            {
                id: passwordField

                width: parent.width
                enabled: !isPasswordSaved

                placeholderText: isPasswordSaved
                    ? "\u25cf\u25cf\u25cf\u25cf\u25cf\u25cf\u25cf\u25cf"
                    : qsTr("Password")

                rightPadding: clearPasswordButton.visible
                    ? clearPasswordButton.width
                    : 0

                showError: displayPasswordError
                echoMode: TextInput.Password
                passwordMaskDelay: 1500
                onTextChanged: credentialsEditor.changed()
                selectionAllowed: false
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhPreferLatin
                activeFocusOnTab: true
                onAccepted: credentialsEditor.accepted()

                onActiveFocusChanged:
                {
                    if (activeFocus)
                        displayPasswordError = false
                }
            }

            IconButton
            {
                id: clearPasswordButton

                width: 48
                height: 48

                icon.source: lp("/images/clear.png")

                opacity: isPasswordSaved ? 1.0 : 0.0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 100 } }
                alwaysCompleteHighlightAnimation: false

                anchors.right: passwordField.right
                anchors.verticalCenter: passwordField.verticalCenter

                onClicked:
                {
                    removeSavedCredentials(authDataModel.localSystemId, loginField.text.trim())
                }
            }
        }

        FieldWarning
        {
            id: passwordErrorPanel
            width: parent.width
            opened: text.length
        }
    }

    ItemSelectionDialog
    {
        id: hostSelectionDialog
        title: qsTr("Hosts")
        currentItem: address
        onCurrentItemChanged: addressField.text = currentItem
    }

    ItemSelectionDialog
    {
        id: userSelectionDialog
        title: qsTr("Users")
        currentItem: login
        onItemActivated: loginField.text = currentItem
    }

    ModelDataAccessor
    {
        id: authenticationDataAccessor
        model: authDataModel
    }

    function focusAddressField()
    {
        addressField.forceActiveFocus()
    }

    function focusCredentialsField()
    {
        if (loginField.text.trim().length > 0)
            passwordField.forceActiveFocus()
        else
            loginField.forceActiveFocus()
    }

    Component.onCompleted:
    {
        if (passwordErrorText)
            displayPasswordError = true
    }
}
