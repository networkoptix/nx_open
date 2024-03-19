// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

TextField
{
    id: searchField

    color: activeFocus
        ? ColorTheme.colors.light4
        : hovered
            ? ColorTheme.colors.light10
            : ColorTheme.colors.light13

    placeholderTextColor: hovered || activeFocus
        ? ColorTheme.colors.light10
        : ColorTheme.colors.light13

    leftPadding: 26
    rightPadding: 26

    ImageButton
    {
        id: clearInput

        visible: !!searchField.text
        focusPolicy: Qt.NoFocus

        anchors
        {
            verticalCenter: parent.verticalCenter
            right: parent.right
            rightMargin: 6
        }

        width: 16
        height: 16

        icon.source: "image://svg/skin/user_settings/clear_text_field.svg"
        icon.width: width
        icon.height: height

        onClicked: searchField.text = ""
    }

    background: Rectangle
    {
        IconImage
        {
            x: 6
            anchors.verticalCenter: parent.verticalCenter

            sourceSize: Qt.size(16, 16)
            source: "image://svg/skin/user_settings/search.svg"
            color: searchField.color
        }

        radius: 2
        color: searchField.activeFocus
            ? ColorTheme.colors.dark8
            : ColorTheme.colors.dark10

        border.width: 1
        border.color: ColorTheme.colors.dark9
    }
}
