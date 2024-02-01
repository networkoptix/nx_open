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

    required property Analytics.StateView taxonomy
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

    signal entriesImported

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

        PreviewTableView
        {
            id: tableView

            anchors.top: linePreview.bottom
            anchors.topMargin: 4
            anchors.bottom: parent.bottom
            width: parent.width
            model: importModel

        }
    }

    buttonBox: DialogButtonBox
    {
        Button
        {
            text: qsTr("Import")
            isAccentButton: true
            enabled: tableView.correct
            onClicked:
            {
                importProcessor.importListEntries(processor.filePath, processor.separator,
                    processor.dataHasHeaderRow, importModel)
            }
        }

        Button
        {
            text: qsTr("Cancel")
            onClicked:
            {
                importProcessor.cancelRunningTask()
                control.close()
            }
        }
    }

    LookupListImportEntriesModel
    {
        id: importModel

        lookupListEntriesModel: control.model
    }

    LookupListPreviewProcessor
    {
        id: processor

        function initImportModel()
        {
            buildTablePreview(importModel, filePath, separator, dataHasHeaderRow)
        }

        rowsNumber: 10
        filePath: filePathField.text
        separator: separatorField.text
        dataHasHeaderRow: headerCheckBox.checked

        onSeparatorChanged: initImportModel()
        onFilePathChanged: initImportModel()
        onDataHasHeaderRowChanged: initImportModel()
    }

    onVisibleChanged:
    {
        separatorField.focus = false
        processor.reset(importModel)
    }

    onRejected:
    {
        fixLookupListImportDialog.reject()
    }

    ProgressDialog
    {
        id: importProgressBar

        title: qsTr("Import List")
        processName: qsTr("Importing")
        visible: false
        onRejected: importProcessor.cancelRunningTask()
        onDoneClicked: control.accept()
    }
    
    FixLookupListImportDialog
    {
        id: fixLookupListImportDialog

        taxonomy: control.taxonomy
        model: importModel
        onAccepted: importProcessor.applyFixUps(importModel)
        onRejected: control.accept()
    }

    LookupListImportProcessor
    {
        id: importProcessor

        onImportStarted:
        {
            importProgressBar.processName = qsTr("Importing")
            importProgressBar.progressStarted()
        }
        onImportFinished: (exitCode) =>
        {
            switch (exitCode)
            {
                case LookupListImportProcessor.Success:
                    importProgressBar.progressFinished()
                    entriesImported()
                    break;
                case LookupListImportProcessor.ErrorFileNotFound:
                    importProgressBar.visible = false
                    MessageBox.exec(MessageBox.Icon.Critical,
                        qsTr("Could not open file"),
                        qsTr("Please ensure that file exists and you have access to selected file"),
                        MessageBox.Ok);
                    break;
                case LookupListImportProcessor.ClarificationRequired:
                    fixLookupListImportDialog.visible = true
                    importProgressBar.visible = false
                    break;
                default:
                    control.reject()
            }
        }

        onFixupStarted:
        {
            importProgressBar.processName = qsTr("Fixing imported entries")
            importProgressBar.progressStarted()
        }

        onFixupFinished: (exitCode) =>
        {
            switch (exitCode)
            {
                case LookupListImportProcessor.Success:
                    importProgressBar.progressFinished()
                    entriesImported()
                    break;
                default:
                    control.reject()
            }
        }

    }
}
