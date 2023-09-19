// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import Qt.labs.qmlmodels 1.0

import Nx
import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

Dialog
{
    id: control

    required property LookupListEntriesModel model
    title: qsTr("Import List")
    minimumWidth: 611
    minimumHeight: 464
    maximumHeight: minimumHeight
    modality: Qt.ApplicationModal
    topPadding: 16
    leftPadding: 15
    bottomPadding: 16
    rightPadding: 15

    function resetTextFieldFocus()
    {
        separatorField.focus = false
    }

    contentItem: Item
    {
        MouseArea
        {
            anchors.fill: parent
            onClicked: resetTextFieldFocus()
        }

        Text
        {
            id: headerOptions

            text: qsTr("Import Options")
            font.pixelSize: 16
            font.weight: Font.Medium
            color: ColorTheme.colors.light4
        }

        Rectangle
        {
            id: lineImp

            anchors.top: headerOptions.bottom
            anchors.topMargin: 4
            width: parent.width
            height: 1

            color: ColorTheme.colors.dark12
        }

        GridLayout
        {
            id: optionsGrid

            anchors.top: lineImp.bottom
            anchors.topMargin: 8
            x: 23
            width: parent.width - x

            rowSpacing: 8
            columnSpacing: 8
            columns: 3
            rows: 4

            Label
            {
                text: qsTr("File:")
                Layout.alignment: Qt.AlignRight
            }

            TextField
            {
                id: filePathField

                Layout.fillWidth: true
                text: processor.filePath
                readOnly: true
            }

            Button
            {
                text: qsTr("Browse...")
                Layout.alignment: Qt.AlignRight
                onClicked:
                {
                    resetTextFieldFocus()
                    processor.setImportFilePathFromDialog()
                }
            }

            Label
            {
                text: qsTr("Separator:")
                Layout.alignment: Qt.AlignRight
            }

            TextField
            {
                id: separatorField

                Layout.maximumWidth: 40

                text: processor.separator
                maximumLength: 1
                Keys.onPressed: (event) =>
                {
                    event.accepted = true
                    focus = true
                    switch (event.key)
                    {
                        case Qt.Key_Enter:
                        case Qt.Key_Return:
                            editingFinished()
                            focus = false
                            break

                        case Qt.Key_Escape:
                            undo()
                            focus = false
                            break

                        case Qt.Key_Tab:
                            text = '\t'
                            break

                        default:
                            event.accepted = false
                            break
                    }
                }

                Keys.onShortcutOverride: (event) =>
                {
                    switch (event.key)
                    {
                        case Qt.Key_Enter:
                        case Qt.Key_Return:
                        case Qt.Key_Escape:
                        case Qt.Key_Tab:
                            event.accepted = true
                            break
                    }
                }
            }

            CheckBox
            {
                id: headerCheckBox

                Layout.row: 3
                Layout.column: 1

                checked: processor.dataHasHeaderRow
                text: qsTr("Data contains header")
                onCheckedChanged: resetTextFieldFocus()
            }
        }

        Text
        {
            id: headerPreview

            anchors.top: optionsGrid.bottom
            anchors.topMargin: 16

            text: qsTr("Preview")
            font.pixelSize: 16
            font.weight: Font.Medium
            color: ColorTheme.colors.light4
        }

        Rectangle
        {
            id: linePreview

            anchors.top: headerPreview.bottom
            anchors.topMargin: 4
            width: parent.width
            height: 1

            color: ColorTheme.colors.dark12
        }

        //TODO: need to add delegate for header in TableView
        TableView
        {
            id: tableView

            anchors.top: linePreview.bottom
            anchors.topMargin: 4
            anchors.bottom: parent.bottom
            width: parent.width

            horizontalHeaderVisible: true
            model: previewModel

            delegate: BasicTableCellDelegate
            {
                color: ColorTheme.colors.light10
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        Button
        {
            text: qsTr("Import")
            isAccentButton: true
            onClicked: control.accept()
        }

        Button
        {
            text: qsTr("Cancel")
            onClicked: control.close()
        }
    }

    LookupPreviewEntriesModel
    {
        id: previewModel
    }

    LookupListPreviewProcessor
    {
        id: processor

        function initPreviewModel()
        {
            previewModel.setHeaderRowData(control.model)
            buildTablePreview(previewModel, filePath, separator, dataHasHeaderRow)
        }

        rowsNumber: 10
        filePath: filePathField.text
        separator: separatorField.text
        dataHasHeaderRow: headerCheckBox.checked

        onSeparatorChanged: initPreviewModel()
        onFilePathChanged: initPreviewModel()
        onDataHasHeaderRowChanged: initPreviewModel()
    }

    onVisibleChanged:
    {
        separatorField.focus = false
        processor.reset(previewModel)
    }
}
