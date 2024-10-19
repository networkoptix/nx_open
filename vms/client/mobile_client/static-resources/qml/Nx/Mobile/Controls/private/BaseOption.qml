// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

/**
* Implements basic option control implementation. It contains option text, description and area
* for the custom control and implements basic behavior like standard drawing and click handlig.
*/
Control
{
    id: control

    property alias text: textItem.text
    property alias textItem: textItem

    property alias descriptionText: descriptionTextItem.text
    property alias descriptionTextItem: descriptionTextItem

    property alias customArea: customAreaLoader.sourceComponent

    implicitHeight: topPadding + bottomPadding + contentItem.height
    implicitWidth: (parent && parent.width) ?? 0

    spacing: 16
    topPadding: 18
    bottomPadding: 18
    leftPadding: 16
    rightPadding: 16

    signal clicked()

    background: Rectangle
    {
        color: ColorTheme.colors.dark6

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            rippleSize: 160
            mouseArea: backgroudMouseArea
        }

        MouseArea
        {
            id: backgroudMouseArea

            anchors.fill: parent
            onClicked: control.clicked()
        }
    }

    contentItem: Item
    {
        height: Math.max(textArea.height, customAreaLoader.height)

        Column
        {
            id: textArea

            spacing: 4

            anchors.left: parent.left
            anchors.right: customAreaLoader.visible
                ? customAreaLoader.left
                : parent.right
            anchors.rightMargin: customAreaLoader.visible
                ? control.spacing
                : 0
            anchors.verticalCenter: parent.verticalCenter

            Text
            {
                id: textItem

                text: control.text

                width: parent.width
                color: ColorTheme.windowText
                font.pixelSize: 16
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }

            Text
            {
                id: descriptionTextItem

                visible: !!text
                text: control.descriptionText

                width: parent.width
                color: ColorTheme.colors.light12
                font.pixelSize: 13
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }
        }

        Loader
        {
            id: customAreaLoader

            visible: !!item
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
