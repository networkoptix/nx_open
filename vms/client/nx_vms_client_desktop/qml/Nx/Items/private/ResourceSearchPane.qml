// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

FocusScope
{
    id: resourceSearchPane

    property bool menuEnabled: false
    property var isFilterRelevant: (type => true)

    readonly property string filterText: searchField.text
    readonly property int filterType: searchField.filterType

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    signal enterPressed(int modifiers)

    function clear()
    {
        searchField.clear()
        tagButton.deactivate()
    }

    Column
    {
        id: column

        spacing: 1
        bottomPadding: 8
        leftPadding: 8
        rightPadding: 8
        topPadding: 8
        width: resourceSearchPane.width
        height: resourceSearchPane.height

        SearchField
        {
            id: searchField

            property var filterType: ResourceTree.FilterType.noFilter

            spacing: menuEnabled ? 4 : 2
            darkMode: true
            height: 32
            width: parent.width - parent.leftPadding - parent.rightPadding
            focus: true

            menu: menuEnabled ? menuInstance : null

            Keys.onPressed: (event) =>
            {
                if (event.key !== Qt.Key_Enter && event.key !== Qt.Key_Return)
                    return

                resourceSearchPane.enterPressed(event.modifiers)
                event.accepted = true
            }

            Keys.onShortcutOverride: (event) =>
                event.accepted = event.key === Qt.Key_Enter || event.key === Qt.Key_Return

            Menu
            {
                id: menuInstance

                component MenuAction: Action
                {
                    visible: menuInstance.shown, //< To re-evaluate upon each show.
                        !data || resourceSearchPane.isFilterRelevant(data)
                }

                MenuAction { text: qsTr("Servers"); data: ResourceTree.FilterType.servers }
                MenuAction { text: qsTr("Cameras & Devices"); data: ResourceTree.FilterType.cameras }
                MenuAction { text: qsTr("Layouts"); data: ResourceTree.FilterType.layouts }
                MenuAction { text: qsTr("Showreels"); data: ResourceTree.FilterType.showreels }
                MenuAction { text: qsTr("Video Walls"); data: ResourceTree.FilterType.videowalls }
                MenuAction
                {
                    text: qsTr("Integrations")
                    data: ResourceTree.FilterType.integrations
                    visible: LocalSettings.iniConfigValue("webPagesAndIntegrations")
                }
                MenuAction { text: qsTr("Web Pages"); data: ResourceTree.FilterType.webPages }
                MenuAction { text: qsTr("Local Files"); data: ResourceTree.FilterType.localFiles }

                onTriggered: (action) =>
                {
                    searchField.filterType = action.data
                    searchTag.text = action.text
                }

                onAboutToHide:
                    searchField.forceActiveFocus()
            }
        }

        Item
        {
            id: searchTag

            property alias text: tagButton.text

            width: parent.width
            height: tagButton.height + 8
            visible: !!text

            TagButton
            {
                id: tagButton

                x: 8
                y: 8

                onCleared:
                    searchField.filterType = ResourceTree.FilterType.noFilter

                Connections
                {
                    target: resourceSearchPane

                    function onMenuEnabledChanged()
                    {
                        if (!resourceSearchPane.menuEnabled)
                            tagButton.setState(SelectableTextButton.State.Deactivated)
                    }
                }
            }
        }
    }
}
