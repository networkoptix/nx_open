// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import Nx.Core
import Nx.Models
import Nx.Mobile
import Nx.Mobile.Controls

Item
{
    id: control

    property alias resource: model.resource
    property int currentIndex: -1
    readonly property bool hasCurrentIndex: currentIndex !== -1

    implicitHeight: 64

    signal selected(int index)

    PtzPresetModel
    {
        id: model
    }

    ModelDataAccessor
    {
        id: modelAccessor

        model: model
    }

    Text
    {
        anchors.centerIn: parent

        color: hasCurrentIndex ? ColorTheme.colors.light4 : ColorTheme.colors.light10
        font.pixelSize: 16
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
        onClicked: sheet.open()
    }

    Button
    {
        id: previousButton

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter

        enabled: currentIndex > 0
        background: null
        padding: 20
        icon.source: "image://skin/24x24/Outline/arrow_left_2px.svg"
        icon.width: 24
        icon.height: 24

        onClicked:
        {
            currentIndex--
            control.selected(currentIndex)
        }
    }

    Button
    {
        id: nextButton

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        enabled: currentIndex < modelAccessor.count - 1
        background: null
        padding: 20
        icon.source: "image://skin/24x24/Outline/arrow_right_2px.svg"
        icon.width: 24
        icon.height: 24

        onClicked:
        {
            currentIndex++
            control.selected(currentIndex)
        }
    }

    PresetSheet
    {
        id: sheet

        model: model
        highlightIndex: control.currentIndex
        onSelected: (index) =>
        {
            control.currentIndex = index
            control.selected(currentIndex)
        }
    }
}
