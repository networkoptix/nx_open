// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0 as Nx

import "private"

/**
 * Interactive Settings type.
 */
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

            focusPolicy: Qt.NoFocus

            icon.color: hovered ? ColorTheme.colors.light1: ColorTheme.colors.light4
            icon.source: showPassword.checked
                ? "image://svg/skin/text_buttons/eye_open_20.svg"
                : "image://svg/skin/text_buttons/eye_closed_20.svg"
        }
    }
}
