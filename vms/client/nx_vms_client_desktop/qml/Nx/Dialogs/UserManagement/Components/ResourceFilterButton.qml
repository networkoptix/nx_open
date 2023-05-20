// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls.impl 2.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.Core 1.0

import nx.vms.client.desktop 1.0

Item
{
    id: control

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    readonly property int resourceTypeFilter: button.selection || ResourceTree.FilterType.noFilter

    readonly property bool withPermissionsOnly:
        button.selection === ResourceTree.FilterType.noFilter

    Button
    {
        id: button

        anchors.fill: control
        hoverEnabled: true
        menu: menuInstance

        property var selection: undefined

        background: Rectangle
        {
            radius: 3

            color: button.down || menuInstance.shown
                ? ColorTheme.colors.dark5
                : button.hovered ? ColorTheme.colors.dark8 : "transparent"

            border.color: button.down || menuInstance.shown
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

        PlatformMenu
        {
            id: menuInstance

            component MenuAction: Action
            {
                checked: button.selection === data
            }

            MenuAction { text: qsTr("With Permissions"); data: 0 }

            PlatformMenuSeparator {}

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

            onTriggered: (action) =>
                button.selection = action.checked ? undefined : action.data

            onAboutToHide:
                searchField.forceActiveFocus()
        }
    }
}
