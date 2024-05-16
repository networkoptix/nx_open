// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQml 2.14

import Nx.Core 1.0
import Nx.Controls 1.0

import "private"

CollapsiblePanel
{
    id: control

    property alias model: repeater.model
    property var selectedTypeId: null

    property string textRole: "name"
    property string iconRole: "icon"
    property string idRole: "id"

    property var iconPath: (iconName => iconName)

    value:
    {
        const button = Array.prototype.find.call(grid.children,
            (item) => ((item instanceof OptionButton) && item.selected))

        return button ? button.text : ""
    }

    function clear()
    {
        if (selectedTypeId)
            selectedTypeId = null
    }

    onClearRequested:
        clear()

    contentItem: Grid
    {
        id: grid

        spacing: 2

        readonly property real kMinimumCellWidth: 90

        readonly property real cellWidth:
        {
            const availableWidth = grid.width - grid.leftPadding - grid.rightPadding
            return Math.round(
                (availableWidth - (grid.columns - 1) * grid.spacing) / grid.columns)
        }

        columns: Math.min(
            (grid.width + grid.spacing) / (grid.kMinimumCellWidth + grid.spacing), repeater.count)

        Repeater
        {
            id: repeater

            OptionButton
            {
                readonly property var data: Array.isArray(repeater.model)
                    ? modelData
                    : model

                readonly property var id: data[idRole]

                horizontal: grid.columns === 1

                text: data[textRole]
                width: grid.cellWidth
                selected: id === control.selectedTypeId
                visible: (selected || !control.selectedTypeId || grid.columns > 1)

                icon.source: iconPath && typeof iconPath === "function"
                    ? iconPath(data[iconRole])
                    : data[iconRole]

                icon.width: 20
                icon.height: 20

                onClicked:
                    control.selectedTypeId = selected ? null : id
            }
        }
    }
}
