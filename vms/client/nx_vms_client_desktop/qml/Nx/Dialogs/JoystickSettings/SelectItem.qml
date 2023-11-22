// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

Rectangle
{
    property alias text: label.text
	property alias image: image.source

    readonly property real contentWidth:
        labelLayout.x + labelLayout.contentWidth + labelLayout.anchors.rightMargin

    property bool highlighted: false

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

        readonly property real contentWidth: label.contentWidth

        anchors.left: image.right
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16

        height: parent.height

        Label
        {
            id: label

            anchors.verticalCenter: parent.verticalCenter

            elide: Text.ElideRight
            font.pixelSize: FontConfig.normal.pixelSize
            color: ColorTheme.colors.light10
        }
    }
}
