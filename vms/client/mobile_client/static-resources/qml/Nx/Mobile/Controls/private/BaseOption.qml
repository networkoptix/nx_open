// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Controls
import Nx.Ui

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
    property alias customAreaItem: customAreaLoader.item

    implicitWidth: (parent && parent.width) ?? 0

    spacing: 16
    topPadding: 12
    bottomPadding: 12
    leftPadding: 16
    rightPadding: 16

    signal clicked()

    background: Rectangle
    {
        color: ColorTheme.colors.dark6
        radius: LayoutController.isTabletLayout ? 8 : 0

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
        implicitHeight: textArea.implicitHeight
        implicitWidth: textArea.implicitWidth

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
                id: descriptionTextItem

                visible: !!text
                text: control.descriptionText

                width: parent.width
                color: ColorTheme.colors.light16
                font.pixelSize: 12
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }

            Text
            {
                id: textItem

                text: control.text

                width: parent.width
                color: ColorTheme.colors.light10
                font.pixelSize: 18
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
