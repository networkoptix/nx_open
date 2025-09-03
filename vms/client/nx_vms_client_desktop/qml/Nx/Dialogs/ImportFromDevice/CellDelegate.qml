// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as T
import QtQuick.Layouts
import QtQml

import Nx.Controls
import Nx.Core.Controls

import nx.vms.client.desktop

Item
{
    signal doubleClicked(int row)

    implicitHeight: 26

    clip: true

    T.IconImage
    {
        id: iconItem

        x: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        sourceSize: Qt.size(width, height)

        source: model.iconKey ? "image://resource/" + model.iconKey : ""

        color: model.foregroundColor

        visible: column == 0
    }

    Text
    {
        x: iconItem.x + (iconItem.visible ? iconItem.width + 6 : 0)
        width: parent.width - x - 8
        anchors.verticalCenter: parent.verticalCenter

        elide: Text.ElideRight
        font.pixelSize: 14

        color: model.foregroundColor

        text: model.display
    }

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onDoubleClicked: parent.doubleClicked(model.row)
    }
}
