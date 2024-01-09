// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx 1.0
import Nx.Core 1.0
import Nx.Dialogs 1.0

import "private"

/**
 * Interactive Settings type.
 */
LabeledItem
{
    id: control

    property var defaultValue

    // Initialized from defaultValue in Component.onCompleted.
    property size minimum
    property size maximum
    property point minBoxPosition
    property point maxBoxPosition

    signal valueChanged()

    isGroup: true

    contentItem: Item
    {
        implicitWidth: figureView.implicitWidth
        implicitHeight: figureView.implicitHeight

        FigureView
        {
            id: figureView

            figure: control.getValue()
            figureType: "size_constraints"

            onEditRequested:
                openEditDialog()

            onFigureChanged:
                control.valueChanged()
        }
    }

    Loader
    {
        id: dialogLoader

        active: false

        sourceComponent: Component
        {
            FigureEditorDialog
            {
                figureType: figureView.figureType
                resourceId: sharedData.resourceId
                player.videoQuality:
                    sharedData.analyticsStreamQuality ?? MediaPlayer.LowVideoQuality

                onAccepted:
                    setValue(serializeFigure())
            }
        }
    }

    function openEditDialog()
    {
        dialogLoader.active = true
        let dialog = dialogLoader.item
        dialog.title = control.caption || qsTr("Size Constraints")
        dialog.showClearButton = false
        dialog.showPalette = false
        dialog.deserializeFigure(getValue())
        dialog.show()
    }

    function getValue()
    {
        return {
            "minimum": [minimum.width, minimum.height],
            "maximum": [maximum.width, maximum.height],
            "positions": [
                [minBoxPosition.x, minBoxPosition.y],
                [maxBoxPosition.x, maxBoxPosition.y]
            ]
        }
    }

    function setValue(value)
    {
        const kDefaultMinSize = 0
        const kDefaultMaxSize = 1

        minimum = deserializeSize(value && value.minimum, kDefaultMinSize, Qt.size(0, 0))
        maximum = deserializeSize(value && value.maximum, kDefaultMaxSize, minimum)

        minBoxPosition = deserializePosition(
            value && Array.isArray(value.positions) && value.positions[0], minimum)

        maxBoxPosition = deserializePosition(
            value && Array.isArray(value.positions) && value.positions[1], maximum)

        valueChanged()
        updatePreview()
    }

    function resetValue()
    {
        setValue(null)
    }

    function updatePreview()
    {
        figureView.figure = getValue()
    }

    function deserializeSize(sizeJson, defaultSize, minimumSize)
    {
        let size = Array.isArray(sizeJson)
            ? Qt.size(sizeJson[0] || defaultSize, sizeJson[1] || defaultSize)
            : Qt.size(defaultSize, defaultSize)

        size.width = MathUtils.bound(minimumSize.width, size.width, 1)
        size.height = MathUtils.bound(minimumSize.height, size.height, 1)

        return size
    }

    function deserializePosition(positionJson, size)
    {
        const defaultPos = Qt.point((1 - size.width) / 2, (1 - size.height) / 2)
        if (!Array.isArray(positionJson))
            return defaultPos

        const pos = Qt.point(
            Utils.getValue(positionJson[0], defaultPos.x),
            Utils.getValue(positionJson[1], defaultPos.y))

        return (pos.x >= 0 && pos.x + size.width <= 1 && pos.y >= 0 && pos.y + size.height <= 1)
            ? pos
            : defaultPos
    }

    Component.onCompleted:
        setValue(defaultValue)
}
