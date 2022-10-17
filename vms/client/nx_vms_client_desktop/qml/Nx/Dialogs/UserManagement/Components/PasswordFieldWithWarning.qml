// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

TextFieldWithValidator
{
    id: control

    property bool showStrength: true
    property bool showPassword: false
    readonly property bool strengthAccepted: strength.accepted

    textField.echoMode: showPassword ? TextInput.Normal : TextInput.Password
    textField.rightPadding: 8
        + (showPasswordButton.visible ? 20 + 5 : 0)
        + (indicator.visible ? indicator.width + 5 : 0)

    property var strength: UserSettingsGlobal.passwordStrength(text)

    Rectangle
    {
        id: indicator
        visible: showStrength && textField.text
        anchors.verticalCenter: textField.verticalCenter
        anchors.right: showPasswordButton.left
        anchors.rightMargin: 5
        radius: 2.5

        width: Math.max(indicatorText.width + 8, 40)
        height: 16

        color: strength.background
        GlobalToolTip.text: strength.hint

        Text
        {
            id: indicatorText

            color: ColorTheme.colors.dark5
            anchors.centerIn: parent
            text: strength.text
            font: Qt.font({pixelSize: 10, weight: Font.Bold})
        }
    }

    ImageButton
    {
        id: showPasswordButton

        background: null

        visible: textField.text

        width: 20

        anchors.verticalCenter: textField.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 5

        CursorOverride.shape: Qt.ArrowCursor
        CursorOverride.active: hovered

        icon.color: hovered ? ColorTheme.highlight: ColorTheme.buttonText
        icon.source: control.showPassword
            ? "image://svg/skin/text_buttons/eye_open.svg"
            : "image://svg/skin/text_buttons/eye_close.svg"
        icon.width: 20
        icon.height: 20

        onClicked: control.showPassword = !control.showPassword
    }
}
