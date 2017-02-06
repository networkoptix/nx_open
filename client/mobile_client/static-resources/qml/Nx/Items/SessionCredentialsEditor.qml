import QtQuick 2.6
import Qt.labs.controls 1.0
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
    property bool displayUserCredentialsError: false
    property alias addressErrorText: addressErrorPanel.text
    property alias credentialsErrorText: credentialsErrorPanel.text

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
            onAccepted: KeyNavigation.tab.forceActiveFocus()
            rightPadding: chooseHostButton.visible ? chooseHostButton.width : 8

            IconButton
            {
                id: chooseHostButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                icon: lp("/images/expand.png")
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
            showError: displayUserCredentialsError
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
                icon: lp("/images/expand.png")
                onClicked: userSelectionDialog.open()
                visible: authenticationDataAccessor.count > 1
            }

            onAccepted: KeyNavigation.tab.forceActiveFocus()
            onActiveFocusChanged:
            {
                if (activeFocus)
                    displayUserCredentialsError = false
            }
        }

        TextField
        {
            id: passwordField

            width: parent.width
            placeholderText: qsTr("Password")
            showError: displayUserCredentialsError
            echoMode: TextInput.Password
            passwordMaskDelay: 1500
            onTextChanged: credentialsEditor.changed()
            selectionAllowed: false
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData | Qt.ImhHiddenText
            activeFocusOnTab: true
            onAccepted: credentialsEditor.accepted()
            onActiveFocusChanged:
            {
                if (activeFocus)
                    displayUserCredentialsError = false
            }
        }
        FieldWarning
        {
            id: credentialsErrorPanel;
            width: parent.width
            opened: displayUserCredentialsError
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

    function focusLoginField()
    {
        loginField.forceActiveFocus()
    }
}
