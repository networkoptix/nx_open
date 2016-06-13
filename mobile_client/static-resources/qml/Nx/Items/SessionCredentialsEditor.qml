import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx.Controls 1.0

Pane
{
    id: credentialsEditor

    implicitHeight: content.height
    implicitWidth: parent ? parent.width : 400
    background: null

    property alias address: addressField.text
    property alias login: loginField.text
    property alias password: passwordField.text

    property bool displayAddressError: false
    property bool displayUserCredentialsError: false

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
            onAccepted: KeyNavigation.tab.forceActiveFocus()
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
            Component.onCompleted:
            {
                if (Qt.platform.os == "android")
                    passwordCharacter = "\u2022"
            }
        }
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
