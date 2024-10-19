// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Controls 1.0

Column
{
    id: control

    property alias password: passwordField.text
    property alias placeholderText: passwordField.placeholderText
    readonly property alias hasError: passwordField.showError
    property var clearPasswordHandler

    function forceActiveFocus()
    {
        passwordField.forceActiveFocus()
    }

    Item
    {
        width: parent.width
        height: passwordField.height

        TextField
        {
            id: passwordField

            width: parent.width

            placeholderText: qsTr("Password")

            rightPadding: clearPasswordButton.visible
                ? clearPasswordButton.width
                : 0

            echoMode: TextInput.Password
            passwordMaskDelay: 1500
            selectionAllowed: false
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhPreferLatin
                | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            activeFocusOnTab: true

            onActiveFocusChanged:
            {
                if (activeFocus || !enabled)
                {
                    passwordField.showError = false
                    passwordErrorPanel.text = ""
                }
                else if (passwordField.text.trim().length === 0)
                {
                    passwordErrorPanel.text = qsTr("Password field cannot be empty")
                    passwordField.showError = true
                }
            }
        }

        IconButton
        {
            id: clearPasswordButton

            width: 48
            height: 48

            icon.source: lp("/images/clear.png")

            opacity: passwordField.length ? 1.0 : 0.0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 100 } }
            alwaysCompleteHighlightAnimation: false

            anchors.right: passwordField.right
            anchors.verticalCenter: passwordField.verticalCenter

            onClicked:
            {
                passwordField.clear()
                if (clearPasswordHandler)
                    clearPasswordHandler()
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
