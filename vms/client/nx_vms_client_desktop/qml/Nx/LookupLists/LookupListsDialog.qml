// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

Dialog
{
    id: control

    required property SystemContext systemContext
    property Analytics.StateView taxonomy: systemContext.taxonomyManager().createStateView(control)
    // When comboBox is empty, currentValue is undefined. `??` used for suppressing warnings.
    property LookupListModel currentList: listComboBox.currentValue ?? null
    property var dialog: null
    property bool loaded: false
    property bool saving: false
    property bool closing: false
    property bool hasChanges: false
    property bool editing: false

    title: qsTr("Lookup Lists")

    minimumWidth: 800
    minimumHeight: 600
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    ListModel
    {
        id: listsModel
    }

    LookupListEntriesModel
    {
        id: entriesModel
        listModel: currentList
    }

    function createNewList()
    {
        createNewLookupListDialog.createObject(control).openNewIn(control)
    }

    function editCurrentList()
    {
        const properties = { "sourceModel": currentList }
        editLookupListDialog.createObject(control, properties).openNewIn(control)
    }

    // TODO: #sivanov Implement error handling.
    // This function is to be called from the dialog to show message box (e.g. if saving failed).
    function showError(text: string)
    {
        console.warn(text)
    }

    function accept()
    {
        closing = true
        apply()
    }

    function apply()
    {
        let data = []
        for (let i = 0; i < listsModel.count; ++i)
            data.push(listsModel.get(i).value)
        saving = true
        hasChanges = false
        dialog.save(data)
    }

    Connections
    {
        target: dialog

        function onLoadCompleted(data)
        {
            listsModel.clear()
            for (let i = 0; i < data.length; ++i)
            {
                const model = data[i]
                listsModel.append({
                    text: model.data.name,
                    value: model
                })
            }
            loaded = true
            if (listsModel.count)
                listComboBox.currentIndex = 0
            hasChanges = false
        }
    }

    Component
    {
        id: createNewLookupListDialog

        EditLookupListDialog
        {
            transientParent: control.Window.window
            taxonomy: control.taxonomy
            visible: false
            onAccepted:
            {
                listsModel.append({
                    text: viewModel.name,
                    value: viewModel
                })
                listComboBox.currentIndex = listsModel.count - 1
            }
        }
    }

    Component
    {
        id: editLookupListDialog

        EditLookupListDialog
        {
            transientParent: control.Window.window
            taxonomy: control.taxonomy
            visible: false
            onAccepted:
            {
                listsModel.set(listComboBox.currentIndex, {
                    text: viewModel.name,
                    value: viewModel
                })
                hasChanges = true
            }

            onDeleteRequested:
            {
                const idx = listComboBox.currentIndex > 0
                    ? listComboBox.currentIndex - 1
                    : listsModel.count > 1 ? 0 : -1
                listsModel.remove(listComboBox.currentIndex)
                listComboBox.currentIndex = idx
                hasChanges = true
            }
        }
    }

    Component
    {
        id: addEntryDialog

        AddLookupListEntryDialog
        {
            transientParent: control.Window.window
            taxonomy: control.taxonomy
            visible: false
            model: currentList
            onAccepted:
            {
                entriesModel.addEntry(entry)
                hasChanges = true
            }
        }
    }

    Component
    {
        id: preloader
        Item
        {
            NxDotPreloader
            {
                anchors.centerIn: parent
            }
        }
    }

    Component
    {
        id: noListsPage
        Item
        {
            NoListsPage
            {
                anchors.centerIn: parent
                button.onClicked: createNewList()
            }
        }
    }

    Component
    {
        id: mainPage

        Item
        {
            anchors.fill: parent
            anchors.topMargin: listControlPanel.height
            enabled: !!currentList && !saving

            Control
            {
                id: searchPanel
                background: Rectangle { color: ColorTheme.colors.dark7 }
                width: parent.width
                padding: 16

                contentItem: RowLayout
                {
                    spacing: 8
                    Button
                    {
                        text: qsTr("Add")
                        iconUrl: "image://svg/skin/buttons/add_20x20_deprecated.svg"
                        onClicked: addEntryDialog.createObject(control).openNewIn(control)
                    }
                    TextButton
                    {
                        id: deleteItemButton

                        text: qsTr("Delete")
                        visible: false
                        enabled: !editing
                        icon.source: "image://svg/skin/text_buttons/delete_20_deprecated.svg"
                        onClicked:
                        {
                            entriesModel.deleteEntries(tableView.getSelectedRowIndexes())
                            hasChanges = true
                        }
                    }
                    TextButton
                    {
                        text: qsTr("Import")
                        icon.source: "image://svg/skin/text_buttons/import.svg"
                        enabled: !editing
                        onClicked: importDialog.visible = true
                    }
                    TextButton
                    {
                        id: exportListButton

                        text: qsTr("Export")
                        icon.source: "image://svg/skin/text_buttons/export.svg"
                        enabled: !editing
                        onClicked:
                        {
                            exportProcessor.exportListEntries(
                                tableView.getSelectedRowIndexes(),
                                entriesModel)
                        }
                    }
                    Item { Layout.fillWidth: true }
                    SearchField
                    {
                        property var numOfResultOnPage: 20
                        onTextChanged: entriesModel.setFilter(text, numOfResultOnPage)
                    }
                }
            }

            LookupListTable
            {
                id: tableView

                model: entriesModel
                taxonomy: control.taxonomy

                onHeaderCheckStateChanged:
                {
                    const selectedIndexes = getSelectedRowIndexes()
                    if (selectedIndexes.length === 0 && deleteItemButton.visible)
                    {
                        deleteItemButton.visible = false
                    }
                    else if (selectedIndexes.length !== 0 && !deleteItemButton.visible)
                    {
                        deleteItemButton.visible = true
                    }
                }

                onEditingStarted: editing = true
                onEditingFinished: editing = false
                onValueChanged: hasChanges = true

                anchors
                {
                    left: parent.left
                    top: searchPanel.bottom
                    bottom: parent.bottom
                    right: parent.right
                    topMargin: -8
                    leftMargin: 16
                    rightMargin: 16
                    bottomMargin: 8
                }
            }

            Label
            {
                id: noEntriesLabel

                anchors.centerIn: tableView
                visible: entriesModel.rowCount === 0
                text: qsTr("No Entries")
                font.pixelSize: 14
                color: ColorTheme.colors.dark17
            }
        }
    }

    Control
    {
        id: listControlPanel
        enabled: loaded
        background: Rectangle
        {
            color: ColorTheme.colors.dark8
        }
        width: control.width
        padding: 16
        contentItem: Row
        {
            spacing: 8
            ComboBox
            {
                id: listComboBox
                model: listsModel
                enabled: !!currentList
                textRole: "text"
                valueRole: "value"
            }
            Button
            {
                text: qsTr("Create New...")
                iconUrl: "image://svg/skin/buttons/add_20x20_deprecated.svg"
                onClicked: createNewList()
            }
            TextButton
            {
                enabled: !!currentList
                text: qsTr("Settings")
                icon.source: "image://svg/skin/text_buttons/settings_20.svg"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: editCurrentList()
            }
        }
    }

    LookupListImportDialog
    {
        id: importDialog

        model: entriesModel
        visible: false
        onEntriesImported: hasChanges = true
    }

    contentItem: Loader
    {
        sourceComponent: !loaded
            ? preloader
            : currentList ? mainPage : noListsPage
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Apply | DialogButtonBox.Cancel

        Component.onCompleted:
        {
            let applyButton = buttonBox.standardButton(DialogButtonBox.Apply)
            applyButton.enabled = Qt.binding(function() { return hasChanges })
        }
    }

    ExportEntriesProgressDialog
    {
        id: exportProgressBar

        visible: false
        onRejected: exportProcessor.cancelExport()
        onOpenFolderRequested: Qt.openUrlExternally(exportProcessor.exportFolder())
    }

    LookupListExportProcessor
    {
        id: exportProcessor

        onExportStarted: exportProgressBar.progressStarted()
        onExportFinished: (exitCode) =>
        {
            switch (exitCode)
            {
                case LookupListExportProcessor.Success:
                    exportProgressBar.progressFinished()
                    break;
                case LookupListExportProcessor.ErrorFileNotOpened:
                    exportProgressBar.visible = false
                    MessageBox.exec(MessageBox.Icon.Critical,
                        qsTr("Could not save file"),
                        qsTr("Please ensure that you have access to selected folder and enough disk space"),
                        MessageBox.Ok);
                    break;
                default:
                    exportProgressBar.visible = false
            }
        }
    }
}
