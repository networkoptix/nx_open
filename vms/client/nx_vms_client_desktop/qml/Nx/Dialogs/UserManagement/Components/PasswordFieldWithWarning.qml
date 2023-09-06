// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

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

    property bool showFakePassword: false

    textField.placeholderTextColor: showFakePassword
        ? (focus ? textField.selectedTextColor : textField.color)
        : ""
    textField.placeholderText: !showFakePassword
        ? ""
        : textField.passwordCharacter.repeat(10)
    textField.hidePlaceholderOnFocus: !showFakePassword
    textField.selectPlaceholderOnFocus: showFakePassword

    Connections
    {
        target: control.textField
        function onTextEdited() { control.showFakePassword = false }
    }

    Instrument
    {
        item: textField

        onKeyPress: (event) =>
        {
            if (!control.showFakePassword)
                return

            if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace)
                control.showFakePassword = false
        }
    }

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

        focusPolicy: Qt.NoFocus

        CursorOverride.shape: Qt.ArrowCursor
        CursorOverride.active: hovered

        icon.color: hovered ? ColorTheme.highlight: ColorTheme.buttonText
        icon.source: control.showPassword
            ? "image://svg/skin/text_buttons/eye_open.svg"
            : "image://svg/skin/text_buttons/eye_close.svg"

        onClicked: control.showPassword = !control.showPassword
    }
}
