// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

import Nx.Controls
import Nx.Core
import Nx.Mobile.Controls
import Nx.Models

Control
{
    id: control

    property alias model: modelAccessor.model
    property int currentIndex: -1
    readonly property bool hasCurrentIndex: modelAccessor.count > 0 && currentIndex !== -1
    property bool overlayStyle: false

    signal selected(int index)
    signal clicked()

    implicitWidth: 164
    implicitHeight: overlayStyle ? 36 : 24

    topPadding: overlayStyle ? 8 : 0
    bottomPadding: overlayStyle ? 4 : 0
    leftPadding: overlayStyle ? 12 : 0
    rightPadding: overlayStyle ? 12 : 0

    background: Rectangle
    {
        visible: overlayStyle
        radius: 6
        color: ColorTheme.colors.dark8
        opacity: 0.5
    }

    ModelDataAccessor
    {
        id: modelAccessor
    }

    contentItem: Item
    {
        Text
        {
            id: title

            anchors.centerIn: parent

            color: hasCurrentIndex ? ColorTheme.colors.light4 : ColorTheme.colors.light10
            font.pixelSize: overlayStyle ? 14 : 16
            font.weight: hasCurrentIndex ? Font.Normal : Font.Medium
            text: hasCurrentIndex
                ? modelAccessor.getData(currentIndex, "display")
                : qsTr("Select Preset")

            Shape
            {
                id: dotLine

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.bottom

                visible: !hasCurrentIndex

                ShapePath
                {
                    strokeStyle: ShapePath.DashLine
                    dashPattern: [0.001, 2]
                    strokeWidth: 1
                    strokeColor: ColorTheme.colors.light10
                    capStyle: ShapePath.RoundCap

                    PathLine { x: dotLine.width }
                }
            }
        }

        MouseArea
        {
            anchors.fill: parent
            onClicked: control.clicked()
        }

        IconButton
        {
            id: previousButton

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            enabled: currentIndex > 0
            compact: true
            icon.source: "image://skin/24x24/Outline/arrow_left_2px.svg?primary=light10"
            icon.width: 24
            icon.height: 24

            onClicked: control.selected(currentIndex - 1)
        }

        IconButton
        {
            id: nextButton

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            enabled: currentIndex < modelAccessor.count - 1
            compact: true
            icon.source: "image://skin/24x24/Outline/arrow_right_2px.svg?primary=light10"
            icon.width: 24
            icon.height: 24

            onClicked: control.selected(currentIndex + 1)
        }
    }
}
