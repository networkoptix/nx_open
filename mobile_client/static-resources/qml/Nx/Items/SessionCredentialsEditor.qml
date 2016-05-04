import QtQuick 2.6
import Qt.labs.controls 1.0

import "../../controls"

Pane
{
    id: credentialsEditor

    implicitHeight: content.height
    implicitWidth: parent ? parent.width : 400
    background: null

    property alias host: hostField.text
    property alias port: portField.text
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

        Row
        {
            QnTextField
            {
                id: hostField

                width: content.width * 0.6
                placeholderText: qsTr("Host")
                showError: displayAddressError
                onTextChanged: credentialsEditor.changed()
                selectionAllowed: false
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                activeFocusOnTab: true
                onAccepted: KeyNavigation.tab.forceActiveFocus()
            }

            QnTextField
            {
                id: portField

                width: parent.width * 2 / 5 - parent.spacing
                placeholderText: qsTr("Port (optional)")
                showError: displayAddressError
                onTextChanged: credentialsEditor.changed()
                selectionAllowed: false
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 0; top: 32767 }
                activeFocusOnTab: liteMode
                onAccepted: KeyNavigation.tab.forceActiveFocus()
            }
        }

        Item
        {
            width: 1
            height: 24
        }

        QnTextField
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

        QnTextField
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

    function focusHostField()
    {
        hostField.forceActiveFocus()
    }

    function focusLoginField()
    {
        loginField.forceActiveFocus()
    }
}
