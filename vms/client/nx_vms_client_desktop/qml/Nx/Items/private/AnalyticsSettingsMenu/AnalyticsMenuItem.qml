// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0

Column
{
    id: item

    property var engineId
    property var sections
    property string sectionId: ""
    property var settingsModel
    property bool selected: false

    property alias active: menuItem.active
    property alias navigationMenu: menuItem.navigationMenu
    property alias text: menuItem.text

    MenuItem
    {
        id: menuItem

        readonly property int level: sections ? sections.length : 0

        itemId: menu.getItemId(engineId, sections)
        text: (settingsModel && settingsModel.caption) || ""

        height: 28
        leftPadding: 16 + level * 8

        color:
        {
            const color = current || (selected && level === 0)
                ? ColorTheme.colors.light1
                : ColorTheme.colors.light10
            const opacity = active ? 1.0 : 0.3
            return ColorTheme.transparent(color, opacity)
        }

        readonly property bool collapsible:
            !!settingsModel && !!settingsModel.sections && settingsModel.sections.length > 0
                && active

        onClicked:
        {
            if (collapsible)
                sectionsView.collapsed = false

            navigationMenu.currentEngineId = engineId
            navigationMenu.currentSectionPath = sections
            navigationMenu.lastClickedSectionId = sectionId
        }

        MouseArea
        {
            id: expandCollapseButton

            width: 20
            height: parent.height
            anchors.right: parent.right
            acceptedButtons: Qt.LeftButton
            visible: parent.collapsible
            hoverEnabled: true

            ArrowIcon
            {
                anchors.centerIn: parent
                rotation: sectionsView.collapsed ? 0 : 180
                color: expandCollapseButton.containsMouse && !expandCollapseButton.pressed
                    ? ColorTheme.lighter(menuItem.color, 2)
                    : menuItem.color
            }

            onClicked:
            {
                if (selected && !menuItem.current)
                    menuItem.click()

                sectionsView.collapsed = !sectionsView.collapsed
            }
        }
    }

    Column
    {
        id: sectionsView

        property bool collapsed: false

        width: parent.width
        height: collapsed ? 0 : implicitHeight
        clip: true
        visible: active

        Repeater
        {
            model: settingsModel && settingsModel.sections

            Loader
            {
                // Using Loader because QML does not allow recursive components.

                width: parent.width

                Component.onCompleted:
                {
                    const model = settingsModel.sections[index]
                    setSource("AnalyticsMenuItem.qml",
                        {
                            "active": Qt.binding(() => active),
                            "engineId": engineId,
                            "navigationMenu": navigationMenu,
                            "sections": sections.concat(index),
                            "selected": Qt.binding(() => selected),
                            "settingsModel": model,
                            "sectionId": model.name,
                        })
                }
            }
        }
    }
}
