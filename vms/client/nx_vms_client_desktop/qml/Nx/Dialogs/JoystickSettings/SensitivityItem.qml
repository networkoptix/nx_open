// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import Nx.Core 1.0
import Nx.Controls 1.0

Rectangle
{
    id: control

    property alias text: nameLabel.text
	property alias image: image.source

    readonly property real contentWidth:
        labelLayout.x + labelLayout.contentWidth + labelLayout.anchors.rightMargin

    property alias value: slider.value

    property bool highlighted: false

    signal moved

    height: 103

    color: "transparent"
    border.color: highlighted ? ColorTheme.colors.light16 : ColorTheme.colors.dark10
	border.width: 1

    Image
    {
        id: image

        x: 16
        y: 16
        width: 64
        height: 64

        sourceSize: Qt.size(width, height)
    }

    Item
    {
        id: labelLayout

        readonly property real contentWidth:
            Math.max(nameLabel.contentWidth, sensitivityLabel.contentWidth)

        anchors.left: image.right
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        y: 16
        height: parent.height - y

        Label
        {
            id: nameLabel

            elide: Text.ElideRight

            color: ColorTheme.colors.light10
        }

        Label
        {
            id: sensitivityLabel

            anchors.top: nameLabel.bottom
            anchors.topMargin: 8

            elide: Text.ElideRight

            text: qsTr("Sensitivity")
        }

        Slider
        {
            id: slider

            anchors.top: sensitivityLabel.bottom
            anchors.topMargin: 12
            width: labelLayout.width

            from: 0.1
            value: 1
            to: 1

            onMoved: control.moved()
        }
    }
}
