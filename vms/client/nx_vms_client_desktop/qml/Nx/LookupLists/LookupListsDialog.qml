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

Dialog
{
    id: control

    property LookupListsDialogStore store: null
    property var currentList: listComboBox.currentValue
    property var dialog: null
    property bool saving: false
    property bool closing: false

    title: qsTr("Lookup Lists")

    width: 800
    height: 600
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    function createNewList()
    {
        createNewLookupListDialog.createObject(control).openNewIn(control)
    }

    function editCurrentList()
    {
        var dialog = editLookupListDialog.createObject(control)
        dialog.listName = currentList.name
        dialog.columnName = currentList.attributeNames ? currentList.attributeNames[0] : "Value"
        dialog.openNewIn(control)
    }

    function showError(text: string)
    {
        console.log(text)
    }

    function accept()
    {
        saving = true
        closing = true
        dialog.requestSave()
    }

    function apply()
    {
        saving = true
        dialog.requestSave()
    }

    Connections
    {
        target: store
        function onHasChangesChanged()
        {
            const applyButton = buttonBox.standardButton(DialogButtonBox.Apply)
            if (applyButton)
                applyButton.enabled = store.hasChanges
        }
    }

    Component
    {
        id: createNewLookupListDialog

        EditLookupListDialog
        {
            editMode: false
            transientParent: control.Window.window
            visible: false
            onAccepted: store.addNewGenericList(listName, columnName)
        }
    }

    Component
    {
        id: editLookupListDialog

        EditLookupListDialog
        {
            editMode: true
            transientParent: control.Window.window
            visible: false
            onAccepted: store.editCurrentGenericList(listName, columnName)
            onDeleteRequested: store.deleteCurrentList()
        }
    }

    Component
    {
        id: addEntryDialog

        AddLookupListEntryDialog
        {
            transientParent: control.Window.window
            visible: false
            model: store.entryModel
            onAccepted: store.addItem()
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
                        iconUrl: "qrc:///skin/buttons/plus.png"
                        onClicked: addEntryDialog.createObject(control).openNewIn(control)
                    }
                    TextButton
                    {
                        text: qsTr("Import")
                        icon.source: "image://svg/skin/text_buttons/import.svg"
                        icon.width: 20
                        icon.height: 20
                    }
                    TextButton
                    {
                        text: qsTr("Export")
                        icon.source: "image://svg/skin/text_buttons/export.svg"
                        icon.width: 20
                        icon.height: 20
                    }
                    Item { Layout.fillWidth: true }
                    SearchField { }
                }
            }

            LookupListTable
            {
                id: tableView
                model: store.selectedListModel

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
        }
    }

    Control
    {
        id: listControlPanel
        enabled: !!store && store.loaded
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
                model: store.listsModel
                enabled: !!currentList
                textRole: "display"
                valueRole: "value"
                currentIndex: store ? store.listIndex : -1
                onActivated: store.selectList(currentIndex)
            }
            Button
            {
                text: qsTr("Create New...")
                iconUrl: "qrc:///skin/buttons/plus.png"
                onClicked: createNewList()
            }
            TextButton
            {
                enabled: !!currentList
                text: qsTr("Settings")
                icon.source: "qrc:///skin/text_buttons/settings.png"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: editCurrentList()
            }
        }
    }

    contentItem: Loader
    {
        sourceComponent: !store || !store.loaded
            ? preloader
            : currentList ? mainPage : noListsPage
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Apply | DialogButtonBox.Cancel
    }
}
