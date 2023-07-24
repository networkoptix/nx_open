// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Controls
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
        menu: menuInstance
        menuXOffset: width - menuInstance.width

        property var selection: undefined

        background: Rectangle
        {
            radius: 3

            color: button.down || menuInstance.visible
                ? ColorTheme.colors.dark5
                : button.hovered ? ColorTheme.colors.dark8 : "transparent"

            border.color: button.down || menuInstance.visible
                ? ColorTheme.colors.dark4
                : ColorTheme.colors.dark11
        }

        contentItem: Item
        {
            id: content

            implicitWidth: image.implicitWidth
            implicitHeight: image.implicitHeight

            IconImage
            {
                id: image

                sourceSize: Qt.size(24, 20)
                anchors.centerIn: content

                source: button.selection !== undefined
                    ? "image://svg/skin/user_settings/filter_drop_set.svg"
                    : "image://svg/skin/user_settings/filter_drop.svg"
            }
        }

        Menu
        {
            id: menuInstance

            component MenuAction: MenuItem
            {
                id: menu

                property int filter

                action: Action
                {
                    checked: button.selection === menu.filter
                    shortcut: null //< Explicitly hide shortcut spacing.
                }

                icon.source: checked
                    ? "image://svg/skin/user_settings/access_rights/filter_checked.svg"
                    : ""
            }

            MenuAction
            {
                text: qsTr("Available by Permissions")
                filter: 0
                visible: editingEnabled
            }

            MenuSeparator
            {
                visible: editingEnabled
            }

            MenuAction
            {
                text: qsTr("Cameras & Devices")
                filter: ResourceTree.ResourceFilter.camerasAndDevices
            }

            MenuAction
            {
                text: qsTr("Layouts")
                filter: ResourceTree.ResourceFilter.layouts
            }

            MenuAction
            {
                text: LocalSettings.iniConfigValue("webPagesAndIntegrations")
                    ? qsTr("Web Pages & Integrations")
                    : qsTr("Web Pages")

                filter: ResourceTree.ResourceFilter.webPages
                    | ResourceTree.ResourceFilter.integrations
            }

            MenuAction
            {
                text: qsTr("Health Monitors")
                filter: ResourceTree.ResourceFilter.healthMonitors
            }

            MenuAction
            {
                text: qsTr("Video Walls")
                filter: ResourceTree.ResourceFilter.videoWalls
            }

            onTriggered: (source) =>
                button.selection = source.checked ? undefined : source.filter

            onAboutToHide:
                searchField.forceActiveFocus()
        }
    }
}
