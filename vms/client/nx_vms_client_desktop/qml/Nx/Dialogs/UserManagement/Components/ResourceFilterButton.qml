// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core.Controls
import Nx.Core

import nx.vms.client.desktop

Item
{
    id: control

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    property bool editingEnabled: true

    readonly property int resourceTypeFilter: button.selection || ResourceTree.FilterType.noFilter

    readonly property bool withPermissionsOnly:
        button.selection === ResourceTree.FilterType.noFilter || !editingEnabled

    Button
    {
        id: button

        anchors.fill: control
        hoverEnabled: true
        menu: dropDownMenu
        menuXOffset: width - dropDownMenu.width

        property var selection: undefined

        background: Rectangle
        {
            radius: 2

            color: button.hovered && !button.down
                ? ColorTheme.colors.dark12
                : ColorTheme.colors.dark11
        }

        contentItem: Item
        {
            id: content

            implicitWidth: image.implicitWidth
            implicitHeight: image.implicitHeight

            ColoredImage
            {
                id: image

                anchors.centerIn: content
                sourcePath: "image://skin/16x16/Outline/filter.svg"
                sourceSize: Qt.size(16, 16)
                primaryColor: "light10"
            }

            Rectangle
            {
                id: indicatorCircle

                x: image.x + 9
                y: image.y - 4
                width: 11
                height: 11
                radius: 5
                color: ColorTheme.colors.attention.red
                visible: button.selection !== undefined
            }
        }

        Menu
        {
            id: dropDownMenu

            component MenuAction: Action
            {
                checked: button.selection === data
                shortcut: null //< Explicitly hide shortcut spacing.

                icon.source: checked ? "image://skin/20x20/Outline/success.svg" : ""
            }

            MenuAction
            {
                text: qsTr("Available by Permissions")
                visible: editingEnabled
                data: 0
            }

            MenuSeparator
            {
                visible: editingEnabled
            }

            MenuAction
            {
                text: qsTr("Cameras & Devices")
                data: ResourceTree.ResourceFilter.camerasAndDevices
            }

            MenuAction
            {
                text: qsTr("Layouts")
                data: ResourceTree.ResourceFilter.layouts
            }

            MenuAction
            {
                text: LocalSettings.iniConfigValue("webPagesAndIntegrations")
                    ? qsTr("Web Pages & Integrations")
                    : qsTr("Web Pages")

                data: ResourceTree.ResourceFilter.webPages
                    | ResourceTree.ResourceFilter.integrations
            }

            MenuAction
            {
                text: qsTr("Health Monitors")
                data: ResourceTree.ResourceFilter.healthMonitors
            }

            MenuAction
            {
                text: qsTr("Video Walls")
                data: ResourceTree.ResourceFilter.videoWalls
            }

            onTriggered: (source) =>
                button.selection = source.checked ? undefined : source.data

            onAboutToHide:
                searchField.forceActiveFocus()
        }
    }
}
