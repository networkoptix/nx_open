import QtQuick 2.6

import Nx 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

// Warning! Uses "thumbnailProvider", "mediaResourceHelper" and "settingsView" objects from context.
// TODO: #vkutin #dklychkov Avoid it if possible.

LabeledItem
{
    id: control

    property string figureType: ""
    property var figureSettings
    property var figure

    isGroup: true

    signal valueChanged()

    readonly property bool filled: !!figure

    contentItem: Column
    {
        spacing: 8

        baselineOffset: figureNameEdit.baselineOffset

        TextField
        {
            id: figureNameEdit
            width: parent.width
            onTextChanged: valueChanged()
        }

        Row
        {
            spacing: 4

            FigureView
            {
                id: figureView

                figure: control.figure
                figureType: control.figureType

                resourceId: settingsView.resourceId
                engineId: settingsView.engineId

                resourceThumbnailProvider: (typeof thumbnailProvider !== "undefined")
                    ? thumbnailProvider
                    : null

                previewRotation: (typeof mediaResourceHelper !== "undefined") && mediaResourceHelper
                    ? mediaResourceHelper.customRotation
                    : 0

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
                icon.source: "qrc:/skin/text_buttons/trash.png"
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
            text: qsTr("Display on camera")
            enabled: figureView.hasFigure
            onCheckedChanged: valueChanged()
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
                resourceId: figureView.resourceId

                onAccepted: figure = serializeFigure()
            }
        }
    }

    onFigureChanged: valueChanged()

    function openEditDialog()
    {
        dialogLoader.active = true
        var dialog = dialogLoader.item
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
        var obj = {
            "figure": figure ? JSON.parse(JSON.stringify(figure)) : null,
            "label": figureNameEdit.text,
            "showOnCamera": showOnCameraCheckBox.checked
        }
        return obj
    }

    function setValue(value)
    {
        figureNameEdit.text = (value && value.label) || ""
        showOnCameraCheckBox.checked = !value || value.showOnCamera !== false
        figure = value && value.figure
    }

    function resetValue()
    {
        setValue(null)
    }
}
