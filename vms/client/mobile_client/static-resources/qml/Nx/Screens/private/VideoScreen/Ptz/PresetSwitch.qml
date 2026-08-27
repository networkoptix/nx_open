// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    implicitHeight: overlayStyle ? 36 : 24

    topPadding: overlayStyle ? 6 : 0
    bottomPadding: overlayStyle ? 6 : 0
    leftPadding: overlayStyle ? 12 : 0
    rightPadding: overlayStyle ? 12 : 0

    background: MouseArea
    {
        Rectangle
        {
            anchors.fill: parent

            visible: overlayStyle
            radius: 6
            color: ColorTheme.transparent(ColorTheme.colors.dark4, 0.5)
            border.width: 1
            border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
        }

        onClicked: control.clicked()
    }

    ModelDataAccessor
    {
        id: modelAccessor
    }

    contentItem: RowLayout
    {
        spacing: 4

        IconButton
        {
            id: previousButton

            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

            enabled: currentIndex > 0
            compact: true
            icon.source: "image://skin/24x24/Outline/arrow_left_2px.svg?primary=light10"
            icon.width: 24
            icon.height: 24

            onClicked: control.selected(currentIndex - 1)
        }

        Text
        {
            id: title

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignCenter
            Layout.minimumWidth: 40
            Layout.maximumWidth: Math.max(Math.ceil(implicitWidth), Layout.minimumWidth)

            horizontalAlignment: Qt.AlignHCenter

            color: hasCurrentIndex ? ColorTheme.colors.light4 : ColorTheme.colors.light10

            font.pixelSize: overlayStyle ? 14 : 16
            font.weight: overlayStyle ? Font.Medium : Font.Normal
            elide: Text.ElideRight
            text: hasCurrentIndex
                ? modelAccessor.getData(currentIndex, "display")
                : qsTr("Select Preset")

            Shape
            {
                id: dotLine

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                width: title.contentWidth

                visible: overlayStyle || !hasCurrentIndex

                ShapePath
                {
                    strokeStyle: ShapePath.DashLine
                    dashPattern: [0.001, 2]
                    strokeWidth: 1
                    strokeColor: title.color
                    capStyle: ShapePath.RoundCap

                    PathLine { x: dotLine.width }
                }
            }
        }

        IconButton
        {
            id: nextButton

            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

            enabled: currentIndex < modelAccessor.count - 1
            compact: true
            icon.source: "image://skin/24x24/Outline/arrow_right_2px.svg?primary=light10"
            icon.width: 24
            icon.height: 24

            onClicked: control.selected(currentIndex + 1)
        }
    }
}
