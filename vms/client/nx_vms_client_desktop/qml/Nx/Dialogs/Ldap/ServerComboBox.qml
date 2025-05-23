// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import nx.vms.client.desktop 1.0

ComboBox
{
    id: control

    property var selectedServer: NxGlobals.uuid("")

    model: ServerListModel
    {
        id: serverListModel

        onServerRemoved:
        {
            control.currentIndex =
                serverListModel.indexOf(control.selectedServer)
        }
    }

    onCurrentIndexChanged:
    {
        const newServer = serverListModel.serverIdAt(currentIndex)
        if (newServer != control.selectedServer)
            control.selectedServer = newServer
    }

    Binding
    {
        target: control
        property: "currentIndex"
        value: serverListModel.indexOf(control.selectedServer)
    }

    component ServerItem: RowLayout
    {
        id: serverItem

        property bool hasIcon: false
        property alias text: serverText.text

        spacing: 6

        ColoredImage
        {
            Layout.alignment: Qt.AlignVCenter

            visible: serverItem.hasIcon

            sourcePath: "image://skin/20x20/Solid/server.svg"
            primaryColor: "light4"
            sourceSize: Qt.size(20, 20)
        }

        Text
        {
            id: serverText

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            font { pixelSize: 14; weight: Font.Medium }
            color: ColorTheme.colors.light4
            elide: Text.ElideRight
        }
    }

    contentItem: Item
    {
        width: parent.width - indicator.width
        height: parent.height

        ServerItem
        {
            text: serverListModel.serverName(control.currentIndex)
            hasIcon: control.currentIndex != 0

            anchors.fill: parent
            anchors.leftMargin: 6
        }
    }

    delegate: ItemDelegate
    {
        id: popupItem

        height: 24
        width: control.width

        background: Rectangle
        {
            color: highlightedIndex == index ? ColorTheme.colors.brand_core : ColorTheme.colors.dark13
        }

        ServerItem
        {
            anchors.fill: parent
            anchors.leftMargin: 6

            text: model.display
            hasIcon: index != 0
        }
    }
}
