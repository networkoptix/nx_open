// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

Item
{
    id: control

    implicitWidth: Math.min(maximumWidth, layout.implicitWidth + horizontalPadding * 2)
    implicitHeight: layout.implicitHeight
    clip: true

    property alias imageSource: placeholderIcon.sourcePath
    property alias text: placeholderText.text
    property alias description: placeholderDescription.text
    property alias buttonText: button.text
    property alias buttonIcon: button.icon

    property alias imageColor: placeholderIcon.primaryColor
    property alias textColor: placeholderText.color
    property alias descriptionColor: placeholderDescription.color

    property real horizontalPadding: 16
    property real maximumWidth: 342

    signal buttonClicked()

    ColumnLayout
    {
        id: layout

        anchors.centerIn: control

        width: Math.min(control.maximumWidth, control.width) - control.horizontalPadding * 2
        spacing: 0

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

            font.pixelSize: 14
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

            onClicked:
                control.buttonClicked()
        }
    }
}
