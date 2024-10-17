// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import Qt.labs.qmlmodels 1.0

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.core.analytics as Analytics

ModalDialog
{
    id: dialog

    required property Analytics.StateView taxonomy
    required property LookupListEntriesModel model
    property alias filePath: previewProcessor.filePath
    property alias separatorSymbol: previewProcessor.separator
    property bool clarificationRequired: false

    title: qsTr("Import List")
    minimumWidth: 611
    minimumHeight: 551
    maximumHeight: minimumHeight
    modality: Qt.ApplicationModal
    topPadding: 16
    leftPadding: 15
    bottomPadding: 16
    rightPadding: 15

    signal entriesImported

    function showProgressBar(processName)
    {
        importProgressBar.processName = processName
        importProgressBar.progressStarted()
    }

    contentItem: Loader
    {
        sourceComponent: dialog.clarificationRequired ? fixLookupListImportPage : mainPage
    }

    Component
    {
        id: mainPage

        Item
        {
            function resetTextFieldFocus()
            {
                separatorField.focus = false
            }

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
                    text: previewProcessor.filePath
                    readOnly: true

                    onTextChanged:
                    {
                        if (text !== previewProcessor.filePath)
                            previewProcessor.filePath = text
                    }
                }

                Button
                {
                    text: qsTr("Browse...")
                    Layout.alignment: Qt.AlignRight
                    onClicked:
                    {
                        resetTextFieldFocus()
                        previewProcessor.setImportFilePathFromDialog()
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

                    text: previewProcessor.separator
                    maximumLength: 1

                    onTextChanged:
                    {
                        if (text !== previewProcessor.separator)
                            previewProcessor.separator = text
                    }
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

                    checked: previewProcessor.dataHasHeaderRow
                    text: qsTr("Data contains header")

                    onCheckedChanged:
                    {
                        if (checked !== previewProcessor.dataHasHeaderRow)
                            previewProcessor.dataHasHeaderRow = checked
                        resetTextFieldFocus()
                    }
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

            DialogButtonBox
            {
                x: -dialog.leftPadding
                width: dialog.width

                Button
                {
                    text: qsTr("Import")
                    isAccentButton: true
                    enabled: tableView.valid && previewProcessor.valid
                    onClicked:
                    {
                        importProcessor.importListEntries(
                            previewProcessor.filePath,
                            previewProcessor.separator,
                            previewProcessor.dataHasHeaderRow,
                            importModel)
                    }
                }

                Button
                {
                    text: qsTr("Cancel")
                    onClicked:
                    {
                        importProcessor.cancelRunningTask()
                        dialog.close()
                    }
                }
            }

            onVisibleChanged:
            {
                separatorField.focus = false
                previewProcessor.initImportModel()
            }
        }
    }

    LookupListImportEntriesModel
    {
        id: importModel

        lookupListEntriesModel: dialog.model
    }

    LookupListPreviewProcessor
    {
        id: previewProcessor


        function processPreviewBuildExitCode(exitCode)
        {
            previewProcessor.valid = exitCode === LookupListPreviewProcessor.Success

            switch (exitCode)
            {
                case LookupListPreviewProcessor.Success:
                    break;
                case LookupListPreviewProcessor.EmptyFileError:
                    MessageBox.exec(MessageBox.Icon.Critical,
                        qsTr("Could not import selected file"),
                        qsTr("The file appears to be empty."),
                        MessageBox.Ok);
                    break;
                case LookupListPreviewProcessor.ErrorFileNotFound:
                    MessageBox.exec(MessageBox.Icon.Critical,
                        qsTr("Could not open file"),
                        qsTr("Please ensure the selected file exists and you have access."),
                        MessageBox.Ok);
                    break;
                default:
                    break;
            }
        }

        function initImportModel()
        {
            // Had to explicitly check this property, since it can be uninitialized when Dialog is created.
            if (importModel.lookupListEntriesModel)
                processPreviewBuildExitCode(buildTablePreview(importModel, filePath, separator, dataHasHeaderRow))
        }

        rowsNumber: 10

        onSeparatorChanged: initImportModel()
        onFilePathChanged: initImportModel()
        onDataHasHeaderRowChanged: initImportModel()
    }

    ProgressDialog
    {
        id: importProgressBar

        title: qsTr("Import List")
        visible: false
        onRejected: importProcessor.cancelRunningTask()
        onAccepted: dialog.accept()
    }

    Component
    {
        id: fixLookupListImportPage

        FixLookupListImportPage
        {
            taxonomy: dialog.taxonomy
            model: importModel
            onAccepted: importProcessor.applyFixUps(importModel)
            onRejected: clarificationRequired = false
        }
    }

    LookupListImportProcessor
    {
        id: importProcessor

        onImportStarted: showProgressBar(qsTr("Importing"))
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
                    clarificationRequired = true
                    importProgressBar.visible = false
                    break;
                case LookupListImportProcessor.SuccessEmptyImport:
                    importProgressBar.progressFinished()
                    break
                default:
                    dialog.reject()
            }
        }

        onFixupStarted: showProgressBar(qsTr("Fixing imported entries"))

        onFixupFinished: (exitCode) =>
        {
            switch (exitCode)
            {
                case LookupListImportProcessor.Success:
                    importProgressBar.progressFinished()
                    clarificationRequired = false
                    entriesImported()
                    break;
                case LookupListImportProcessor.SuccessEmptyImport:
                    clarificationRequired = false
                    importProgressBar.progressFinished()
                    break
                default:
                    dialog.reject()
            }
        }

    }
}
