// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

AbstractButton
{
    id: control

    property alias textItem: textItem
    property alias customAreaComponent: customArea.sourceComponent

    property alias foregroundColor: coloredIcon.primaryColor

    implicitWidth: (parent && parent.width) ?? 0

    background: Rectangle
    {
        radius: 6
        color: ColorTheme.colors.dark6
        border.color: ColorTheme.colors.dark8

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            radius: parent.radius
            mouseArea: control
            rippleSize: 160
            highlightColor: "#30ffffff"
        }
    }

    contentItem: Item
    {
        id: contentItem

        implicitWidth: control.width
        implicitHeight: mainArea.y * 2 + mainArea.height
            + (customArea.height ? customArea.height + 6 : 0)

        Item
        {
            id: mainArea

            x: 20
            y: 14
            height: 24
            width: parent.width - 2 * x

            ColoredImage
            {
                id: coloredIcon

                anchors.verticalCenter: parent.verticalCenter
                primaryColor: ColorTheme.colors.light4
                sourcePath: control.icon.source
                sourceSize: Qt.size(control.icon.width, control.icon.height)
            }

            Text
            {
                id: textItem

                x: coloredIcon.sourcePath
                   ? coloredIcon.width + 12
                   : 0
                width: parent.width - x
                anchors.verticalCenter: parent.verticalCenter

                text: control.text
                color: coloredIcon.primaryColor
                font.pixelSize: 16
                elide: Text.ElideRight
            }
        }

        Loader
        {
            id: customArea

            x: 20 + (coloredIcon.sourcePath ? coloredIcon.x + coloredIcon.width + 12 : 0)
            y: mainArea.y + mainArea.height + 6
            width: parent.width - 2 * x
        }
    }
}
