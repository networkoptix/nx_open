// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.core

/**
 * Interactive Settings private type.
 */
LabeledItem
{
    id: control

    property string figureType: ""
    property var figureSettings
    property var figure
    property bool useLabelField: true
    property bool useDisplayOnCamera: true
    property var defaultValue //< Not used but required by the backend.

    signal valueChanged(bool activated)

    readonly property bool filled: !!figure

    isGroup: true

    contentItem: Column
    {
        spacing: 8

        baselineOffset: figureNameEdit.baselineOffset

        TextField
        {
            id: figureNameEdit
            width: parent.width
            visible: useLabelField
            onTextChanged: valueChanged(/*activated*/ false)
        }

        Row
        {
            spacing: 4

            FigureView
            {
                id: figureView

                figure: control.figure
                figureType: control.figureType

                Text
                {
                    text: qsTr("Click to add")
                    anchors.centerIn: parent
                    visible: !figureView.hasFigure
                    font.pixelSize: 12
                    color: figureView.hovered
                        ? ColorTheme.colors.light14
                        : ColorTheme.colors.dark13
                }

                onEditRequested:
                    openEditDialog()
            }

            TextButton
            {
                icon.source: "image://skin/20x20/Outline/delete.svg"
                visible: figureView.hasFigure

                onClicked:
                {
                    figure = null
                    figureNameEdit.text = ""
                }
            }
        }

        CheckBox
        {
            id: showOnCameraCheckBox
            text: qsTr("Display on video")
            enabled: figureView.hasFigure
            visible: useDisplayOnCamera
            onCheckedChanged: valueChanged(/*activated*/ true)
            topPadding: 0
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
                figureType: control.figureType
                figureSettings: control.figureSettings
                resource: sharedData.resource
                player.videoQuality:
                    sharedData.analyticsStreamQuality ?? MediaPlayer.LowVideoQuality

                onAccepted:
                {
                    let newFigure = serializeFigure()
                    let newFigureString = JSON.stringify(newFigure, Object.keys(newFigure).sort())
                    let figureString =
                        figure ? JSON.stringify(figure, Object.keys(figure).sort()) : ""

                    if (figureString !== newFigureString)
                        figure = newFigure
                }
            }
        }
    }

    onFigureChanged: valueChanged(/*activated*/ true)

    function openEditDialog()
    {
        dialogLoader.active = true
        let dialog = dialogLoader.item
        dialog.title = figureNameEdit.text || qsTr("Figure")
        dialog.deserializeFigure(figure)
        dialog.show()
        dialog.closing.connect(
            function()
            {
                dialogLoader.active = false
            })
    }

    function getValue()
    {
        // Clone the figure object and fill the rest of fields.
        const obj = {
            "figure": figure ? JSON.parse(JSON.stringify(figure)) : null,
            "label": figureNameEdit.text,
            "showOnCamera": showOnCameraCheckBox.checked
        }
        return obj
    }

    function setValue(value)
    {
        figureNameEdit.text = (value && value.label) || ""
        showOnCameraCheckBox.checked = (value && value.showOnCamera) || false
        figure = value && value.figure
    }

    function resetValue()
    {
        setValue(null)
    }
}
