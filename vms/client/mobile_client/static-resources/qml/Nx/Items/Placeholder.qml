// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

ColumnLayout
{
    id: control

    width: 247

    property alias imageSource: placeholderIcon.sourcePath
    property alias text: placeholderText.text
    property alias description: placeholderDescription.text
    property alias buttonText: button.text
    property alias buttonIcon: button.icon

    signal buttonClicked()

    ColoredImage
    {
        id: placeholderIcon

        sourceSize: Qt.size(64, 64)

        Layout.alignment: Qt.AlignHCenter
    }

    Text
    {
        id: placeholderText

        Layout.topMargin: 24
        Layout.fillWidth: true

        color: ColorTheme.colors.light4

        horizontalAlignment: Text.AlignHCenter

        font.pixelSize: 16
        font.weight: Font.Medium

        textFormat: Text.StyledText
    }

    Text
    {
        id: placeholderDescription

        Layout.topMargin: 16
        Layout.fillWidth: true

        visible: text !== ""
        color: ColorTheme.colors.light12

        horizontalAlignment: Text.AlignHCenter

        font.pixelSize: 12
        font.weight: Font.Normal

        textFormat: Text.StyledText
        wrapMode: Text.WordWrap
    }

    Button
    {
        id: button

        Layout.topMargin: 24
        Layout.alignment: Qt.AlignHCenter

        visible: button.text !== ""
        color: ColorTheme.colors.brand_core
        textColor: ColorTheme.colors.dark1

        icon.width: 24
        icon.height: 24

        onClicked: control.buttonClicked()
    }
}
