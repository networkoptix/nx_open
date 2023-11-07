// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Controls.NavigationMenu

Column
{
    id: root

    property var engines: null
    property var enabledEngines
    required property NavigationMenu navigationMenu
    required property var viewModel
    property int level: 1

    Repeater
    {
        model: engines

        Rectangle
        {
            id: container

            width: parent.width
            height: menuItem.height
            color: isSelected ? ColorTheme.colors.dark9 : "transparent"
            visible: model.visible ?? true

            readonly property bool isSelected: menuItem.engineId
                ? viewModel.currentEngineId === menuItem.engineId
                : viewModel.currentRequestId === menuItem.requestId

            readonly property var settingsModel:
                isSelected ? viewModel.currentEngineSettingsModel : null

            EngineMenuItem
            {
                id: menuItem

                width: parent.width
                level: root.level

                active:
                {
                    if (!enabledEngines || enabledEngines.indexOf(engineId) !== -1)
                        return true

                    // Can be undefined when items are being deleted.
                    const engine = engines[index]
                    if (!engine)
                        return false

                    return engine.isDeviceDependent || engine.isDeviceDependent === undefined
                }

                engineId: model.engineId !== undefined
                    ? (model.engineId !== "" ? NxGlobals.uuid(model.engineId) : null)
                    : modelData.id

                requestId: model.requestId
                sectionId: null
                navigationMenu: root.navigationMenu
                settingsModel: container.settingsModel
                selected: container.isSelected
                text: model.name || modelData.name || "" //< Engine name.
            }
        }
    }
}
