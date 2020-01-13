import QtQuick 2.11

import Nx 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0

Column
{
    id: item

    /**
     * Used context properties:
     *
     * var ctx_modelData
     * var ctx_engineId
     * var ctx_sections[]
     * int ctx_level
     * bool ctx_active
     * bool ctx_selected
     * NavigationMenu ctx_navigationMenu
     */

    MenuItem
    {
        id: menuItem

        // Properties to be used in NavigationMenu.itemClicked(item) signal handler.
        readonly property var engineId: ctx_engineId
        readonly property var sections: ctx_sections
        readonly property int level: ctx_level

        height: 28
        itemId: [engineId.toString()].concat(ctx_sections).join("\n")
        text: ctx_modelData.name
        active: ctx_active
        leftPadding: 16 + ctx_level * 8
        navigationMenu: ctx_navigationMenu
        font.pixelSize: 13

        color:
        {
            var color = current || (ctx_selected && ctx_level == 0)
                ? ColorTheme.colors.light1
                : ColorTheme.colors.light10

            var opacity = ctx_active ? 1.0 : 0.3
            return ColorTheme.transparent(color, opacity)
        }

        readonly property var sectionsModel: level
            ? ctx_modelData.sections
            : ctx_modelData.settingsModel.sections

        readonly property bool collapsible: !!sectionsModel && sectionsModel.length > 0

        onClicked:
        {
            if (collapsible)
                sectionsView.collapsed = false
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
                if (ctx_selected && !menuItem.current)
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

        Repeater
        {
            model: menuItem.sectionsModel

            Loader
            {
                width: parent.width
                source: "AnalyticsMenuItem.qml"

                // New context properties:
                property var ctx_modelData: modelData
                property var ctx_sections: menuItem.sections.concat(index)
                property int ctx_level: menuItem.level + 1

                // Passed through:
                //  ctx_navigationMenu
                //  ctx_engineId
                //  ctx_active
                //  ctx_selected
            }
        }
    }
}
