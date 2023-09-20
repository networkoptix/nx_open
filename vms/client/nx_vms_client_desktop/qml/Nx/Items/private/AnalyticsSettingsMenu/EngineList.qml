// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls.NavigationMenu 1.0

Column
{
    id: root

    property var engines: null
    property var enabledEngines
    required property NavigationMenu navigationMenu
    property int level: 1

    Repeater
    {
        model: engines

        Rectangle
        {
            id: container

            width: parent.width
            height: column.height
            color: isSelected ? ColorTheme.colors.dark9 : "transparent"

            readonly property bool isSelected:
                navigationMenu.viewModel.currentEngineId === menuItem.engineId

            readonly property var settingsModel:
                isSelected ? navigationMenu.viewModel.currentEngineSettingsModel : null

            Column
            {
                id: column
                width: parent.width

                EngineMenuItem
                {
                    id: menuItem

                    width: parent.width
                    level: root.level

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
                    navigationMenu: root.navigationMenu
                    sectionId: null
                    settingsModel: container.settingsModel
                    selected: container.isSelected
                    text: modelData.name || "" //< Engine name.
                }
            }
        }
    }
}
