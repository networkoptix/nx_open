// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Controls 1.0 as Nx

import "private"

TextFieldBase
{
    textFieldItem: TextField
    {
        id: textFieldItem

        textField.echoMode: showPassword.checked ? TextInput.Normal : TextInput.Password

        Nx.Button
        {
            id: showPassword

            checkable: true
            width: 20
            height: 20
            background: null
            visible: textFieldItem.text

            anchors.verticalCenter: textFieldItem.textField.verticalCenter
            anchors.right: textFieldItem.textField.right
            anchors.rightMargin: 5
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0

            icon.color: hovered ? ColorTheme.highlight: ColorTheme.buttonText
            icon.source: showPassword.checked
                ? "image://svg/skin/text_buttons/eye_open.svg"
                : "image://svg/skin/text_buttons/eye_close.svg"
        }
    }
}
