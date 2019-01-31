import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0
import Nx.Models 1.0
import com.networkoptix.qml 1.0

import "private"

Pane
{
    id: credentialsEditor

    implicitHeight: content.height
    implicitWidth: parent ? parent.width : 400
    background: null

    property alias hostsModel: hostSelectionDialog.model
    property alias authenticationDataModel: userSelectionDialog.model
    property alias address: addressField.text
    property alias login: loginField.text
    property alias password: passwordField.text

    property bool displayAddressError: false
    property bool displayLoginError: false
    property bool displayPasswordError: false
    property alias addressErrorText: addressErrorPanel.text
    property alias loginErrorText: loginErrorPanel.text
    property alias passwordErrorText: passwordErrorPanel.text

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
            opened: displayAddressError
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
            opened: displayLoginError && text
        }

        TextField
        {
            id: passwordField

            width: parent.width
            placeholderText: qsTr("Password")
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
        FieldWarning
        {
            id: passwordErrorPanel
            width: parent.width
            opened: displayPasswordError && text
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
        onItemActivated:
        {
            loginField.text = currentItem
            passwordField.text =
                authenticationDataAccessor.getData(currentIndex, "credentials").password
        }
    }

    ModelDataAccessor
    {
        id: authenticationDataAccessor
        model: authenticationDataModel
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
}
