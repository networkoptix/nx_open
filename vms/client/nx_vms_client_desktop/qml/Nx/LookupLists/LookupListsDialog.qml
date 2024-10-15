// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics
import nx.vms.client.core.analytics as Analytics

Dialog
{
    id: control

    required property SystemContext systemContext
    property Analytics.StateView taxonomy: systemContext.taxonomyManager.createStateView(control)
    // When comboBox is empty, currentValue is undefined. `??` used for suppressing warnings.
    property LookupListModel currentList: listComboBox.currentValue ?? null
    property var dialog: null
    property bool loaded: false
    property bool saving: false
    property bool closing: false
    property bool hasChanges: false
    property bool editing: false

    signal forceTableSortingRequired

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

        taxonomy: control.taxonomy
        listModel: currentList
    }

    function clearFilter()
    {
        editing = true
        editing = false
    }

    function iconPathForLookupList(data)
    {
        const listType = taxonomy.objectTypeById(data.data.objectTypeId)
        return Analytics.IconManager.iconUrl(listType ? listType.iconSource : "")
    }

    function createNewList()
    {
        dialog.createListRequested()
    }

    function editCurrentList()
    {
        const properties = { "sourceModel": currentList, "systemContext": systemContext }
        editLookupListDialog.createObject(control, properties).openNewIn(control)
    }

    function importList()
    {
        const initialPath = LookupListPreviewHelper.getImportFilePathFromDialog()
        if (!initialPath)
            return //< User clicked Cancel.

        const properties =
            {
                "model": entriesModel,
                "taxonomy": control.taxonomy,
                "filePath": initialPath,
                "separatorSymbol": initialPath.endsWith(".tsv") ? "\t" : ",",
            }
        importDialog.createObject(control, properties).openNewIn(control)
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
                    decorationPath: iconPathForLookupList(model),
                    value: model
                })
            }
            loaded = true
            if (listsModel.count)
                listComboBox.currentIndex = 0
            hasChanges = false
        }

        function onAppendData(data)
        {
            if (data.length == 0)
                return;

            loaded = false
            const firstModel = data[0]
            for (let i = 0; i < data.length; ++i)
            {
                const model = data[i]
                listsModel.append({
                    text: model.data.name,
                    decorationPath: iconPathForLookupList(model),
                    value: model
                })
            }
            listComboBox.currentIndex = listComboBox.indexOfValue(firstModel)

            loaded = true
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
                listsModel.set(listComboBox.listsModelIndex(), {
                    text: viewModel.name,
                    decoration: iconPathForLookupList(viewModel),
                    value: viewModel
                })

                // Need to check all entries if column with data was removed.
                if (columnWithDataWasRemoved)
                    entriesModel.removeIncorrectEntries()

                control.hasChanges = sourceModelIsChanged
                listComboBox.currentIndex = listComboBox.indexOfValue(viewModel)
            }

            onDeleteRequested:
            {
                const idx = listComboBox.currentIndex > 0
                    ? listComboBox.currentIndex - 1
                    : listsModel.count > 1 ? 0 : -1
                listsModel.remove(listComboBox.listsModelIndex())
                listComboBox.currentIndex = idx
                control.hasChanges = true
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
            Component.onCompleted: editing = true
            onRejected: editing = false
            onAccepted:
            {
                entriesModel.addEntry(entry)
                hasChanges = true
                editing = false
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
                    enabled: !editing

                    Text
                    {
                        visible: tableView.checkedRows.length > 0
                        color: ColorTheme.colors.light4
                        font.pixelSize: 14
                        text: qsTr("%1 selected:").arg(tableView.checkedRows.length)
                    }
                    Button
                    {
                        text: qsTr("Add")
                        visible: tableView.checkedRows.length === 0
                        icon.source: "image://skin/20x20/Outline/add.svg"
                        onClicked: addEntryDialog.createObject(control).openNewIn(control)
                    }
                    TextButton
                    {
                        id: deleteItemButton

                        text: qsTr("Delete")
                        visible: tableView.checkedRows.length > 0
                        icon.source: "image://skin/20x20/Outline/delete.svg"
                        onClicked:
                        {
                            editing = true
                            entriesModel.deleteEntries(tableView.checkedRows)
                            hasChanges = true
                            editing = false
                        }
                    }
                    TextButton
                    {
                        text: qsTr("Import")
                        visible: tableView.checkedRows.length === 0
                        icon.source: "image://skin/20x20/Outline/import.svg"
                        onClicked: importList()
                    }
                    TextButton
                    {
                        id: exportListButton

                        text: tableView.checkedRows.length > 0 ? qsTr("Export Selected") : qsTr("Export")
                        icon.source: "image://skin/20x20/Outline/export.svg"
                        onClicked:
                        {
                            exportProcessor.exportListEntries(tableView.checkedRows, entriesModel)
                        }
                    }
                    Item { Layout.fillWidth: true }
                    SearchField
                    {
                        text: control.editing ? '' : text //< Clearing filter on any editing.
                        onTextChanged: lookupListEntriesFilterModel.setFilterRegularExpression(text)
                    }
                }
            }

            LookupListTable
            {
                id: tableView
                sourceModel: LookupListEntriesSortFilterProxyModel
                {
                    id: lookupListEntriesFilterModel

                    sourceModel: entriesModel
                }
                taxonomy: control.taxonomy
                onEditingChanged: control.editing = tableView.editing
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

                Connections
                {
                    target: control

                    function onForceTableSortingRequired()
                    {
                        tableView.sortTable()
                    }
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

                function listsModelIndex()
                {
                    return sortModel.mapToSource(sortModel.index(listComboBox.currentIndex, 0)).row
                }

                model: SortFilterProxyModel
                {
                    id: sortModel

                    sourceModel: listsModel
                    sortCaseSensitivity: Qt.CaseInsensitive
                    sortRoleName: "text"
                }
                withIconSection: true
                enabled: !!currentList
                textRole: "text"
                valueRole: "value"
                onCurrentIndexChanged: clearFilter()
            }
            Button
            {
                text: qsTr("New List...")
                onClicked: createNewList()
            }
            TextButton
            {
                enabled: !!currentList
                text: qsTr("Settings")
                icon.source: "image://skin/20x20/Outline/settings.svg"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: editCurrentList()
            }
        }
    }

    Component
    {
        id: importDialog

        LookupListImportDialog
        {
            visible: false
            onEntriesImported:
            {
                hasChanges = true
                control.forceTableSortingRequired()
            }
            Component.onCompleted: editing = true
            onRejected: editing = false
            onAccepted: editing = false
        }
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
                case LookupListExportProcessor.ErrorFileNotSaved:
                    exportProgressBar.visible = false
                    MessageBox.exec(MessageBox.Icon.Critical,
                        qsTr("Could not save file"),
                        qsTr("Please ensure that you have access to selected folder and enough disk space"),
                        MessageBox.Ok);
                    break;
                default:
                    exportProgressBar.visible = false
                    MessageBox.exec(MessageBox.Icon.Critical,
                        qsTr("Export failed"),
                        qsTr("Please ensure that you have access to selected folder and enough disk space"),
                        MessageBox.Ok);
            }
        }
    }
}
