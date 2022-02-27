// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx 1.0
import Nx.Controls.NavigationMenu 1.0

import "private/AnalyticsSettingsMenu"

NavigationMenu
{
    id: menu

    property var engines: null
    property var enabledEngines: []
    property var currentEngineId

    /** Settings model for current engine. */
    property var currentEngineSettingsModel: null

    /** A sequence of section numeric indexes in the treeish sections structure. */
    property var currentSectionPath: []

    property string lastClickedSectionId: ""

    implicitWidth: 240

    Repeater
    {
        model: engines

        Rectangle
        {
            id: container

            width: parent.width - menu.scrollBarWidth
            height: column.height
            color: isSelected ? ColorTheme.colors.dark9 : "transparent"

            readonly property bool isSelected: currentEngineId === menuItem.engineId
            readonly property var settingsModel: isSelected ? currentEngineSettingsModel : null

            Column
            {
                id: column
                width: parent.width

                AnalyticsMenuItem
                {
                    id: menuItem

                    width: parent.width

                    active:
                    {
                        if (enabledEngines.indexOf(engineId) !== -1)
                            return true

                        // Can be undefined when items are being deleted.
                        const engine = engines[index]
                        if (!engine)
                            return false

                        return engine.isDeviceDependent || engine.isDeviceDependent === undefined
                    }

                    engineId: modelData.id
                    navigationMenu: menu
                    sections: []
                    settingsModel: container.settingsModel
                    selected: container.isSelected
                    text: modelData.name || "" //< Engine name.
                }

                Rectangle
                {
                    id: separator

                    width: parent.width
                    height: 1
                    color: ColorTheme.colors.dark7

                    Rectangle
                    {
                        y: 1
                        z: 1
                        width: parent.width
                        height: 1
                        color: ColorTheme.colors.dark9
                    }
                }
            }
        }
    }

    function getItemId(engineId, path)
    {
        return engineId.toString() + "|" + path.join(",")
    }
}
