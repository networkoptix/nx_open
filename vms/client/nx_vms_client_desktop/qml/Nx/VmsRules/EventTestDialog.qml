// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import Qt.labs.qmlmodels

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.core
import nx.vms.client.desktop

Dialog
{
    id: root

    property var dialog: null
    property var events: [] //< All the available events.
    property var availableStates: []
    property var eventReasons: []
    property var eventLevels: []
    property var boolStates: ["false", "true"]

    property var servers: {}
    property var devices: {}
    property var users: {}

    readonly property var typeToModel: {
        "state": root.availableStates,
        "eventReason": root.eventReasons,
        "eventLevel": root.eventLevels,
        "bool": root.boolStates,
        "server": root.servers,
        "device": root.devices,
        "user": root.users,
        "devices": root.devices }

    property var properties: []

    width: 700
    height: 400
    minimumWidth: 700
    minimumHeight: 400
    title: qsTr("Event Test")

    contentItem: Item
    {
        ListView
        {
            id: eventsListView
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 170
            clip: true
            model: root.events
            highlight: Rectangle { color: ColorTheme.colors.dark17 }
            highlightMoveDuration: 0
            delegate: ItemDelegate
            {
                text: modelData.display
                width: eventsListView.width
                contentItem: Label
                {
                    text: parent.text
                    color: ColorTheme.colors.light1
                    font.pixelSize: FontConfig.normal.pixelSize
                }

                onClicked:
                {
                    eventsListView.currentIndex = index
                }
            }

            onCurrentIndexChanged:
            {
                eventPropertiesModel.clear()

                if (currentIndex === -1)
                    return

                root.dialog.onEventSelected(root.events[currentIndex].type)
            }
        }

        TableView
        {
            id: eventPropertiesTableView
            anchors.left: eventsListView.right
            anchors.leftMargin: 5
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            columnWidthProvider: function(column)
            {
                return width * (column === 0 ? 0.3 : 0.7)
            }

            model: TableModel
            {
                id: eventPropertiesModel
                TableModelColumn { display: "name"; whatsThis: "type" }
                TableModelColumn { display: "value"; whatsThis: "type" }

                rows: Array.from(root.properties)
            }

            delegate: DelegateChooser
            {
                role: "whatsThis"
                DelegateChoice
                {
                    column: 0
                    BasicSelectableTableCellDelegate{}
                }
                DelegateChoice
                {
                    roleValue: "state"
                    delegate: comboBoxComponent
                }
                DelegateChoice
                {
                    roleValue: "eventReason"
                    delegate: comboBoxComponent
                }
                DelegateChoice
                {
                    roleValue: "eventLevel"
                    delegate: comboBoxComponent
                }
                DelegateChoice
                {
                    roleValue: "bool"
                    delegate: comboBoxComponent
                }
                DelegateChoice
                {
                    roleValue: "device"
                    delegate: resourceDelegate
                }
                DelegateChoice
                {
                    roleValue: "server"
                    delegate: resourceDelegate
                }
                DelegateChoice
                {
                    roleValue: "user"
                    delegate: resourceDelegate
                }
                DelegateChoice
                {
                    roleValue: "devices"
                    delegate: resourcesDelegate
                }
                DelegateChoice
                {
                    roleValue: "seconds"
                    BasicSelectableTableCellDelegate
                    {
                        tableView: eventPropertiesTableView
                        TableView.editDelegate: SpinBox
                        {
                            value: JSON.parse(display)
                            from: 0
                            to: 999
                            editable: true

                            Component.onCompleted:
                            {
                                eventPropertiesTableView.editing = true
                            }

                            Component.onDestruction:
                            {
                                display = JSON.stringify(value)
                                eventPropertiesTableView.editing = false
                            }
                        }
                    }
                }
                DelegateChoice
                {
                    roleValue: "double"
                    BasicSelectableTableCellDelegate
                    {
                        tableView: eventPropertiesTableView
                        TableView.editDelegate: DoubleSpinBox
                        {
                            dValue: JSON.parse(display)
                            dFrom: 0
                            dTo: 999
                            dStepSize: 0.1
                            editable: true

                            Component.onCompleted:
                            {
                                eventPropertiesTableView.editing = true
                            }

                            Component.onDestruction:
                            {
                                display = JSON.stringify(dValue)
                                eventPropertiesTableView.editing = false
                            }
                        }
                    }
                }
                DelegateChoice
                {
                    column: 1
                    BasicSelectableTableCellDelegate
                    {
                        tableView: eventPropertiesTableView
                        TableView.editDelegate: TextField
                        {
                            anchors.fill: parent
                            text: display
                            horizontalAlignment: TextInput.AlignLeft
                            verticalAlignment: TextInput.AlignVCenter

                            Component.onCompleted:
                            {
                                eventPropertiesTableView.editing = true
                                selectAll()
                            }

                            Component.onDestruction:
                            {
                                try
                                {
                                    JSON.parse(text)
                                    display = text
                                }
                                catch (e)
                                {
                                    infoDialog.title = "Invalid value"
                                    infoDialogContent.text = "JSON syntax error"
                                    infoDialog.show()
                                }

                                eventPropertiesTableView.editing = false
                            }
                        }
                    }
                }
            }
        }

        Label
        {
            id: hintLabel
            anchors.left: eventsListView.right
            anchors.leftMargin: 5
            anchors.right: testButton.left
            anchors.rightMargin: 5
            anchors.bottom: parent.bottom
            text: "Turn on enableCreateEventApi flag in the nx_vms_server.ini file to enable " +
               "support on the server side"
            wrapMode: Text.Wrap
        }

        Button
        {
            id: testButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            text: qsTr("Test")

            onClicked:
            {
                eventPropertiesTableView.closeEditor()

                enabled = false

                var result = {}
                for (let i = 0; i < eventPropertiesModel.rowCount; ++i)
                {
                    const row = eventPropertiesModel.rows[i]

                    result[row.name] = JSON.parse(row.value)
                }

                result["type"] = root.events[eventsListView.currentIndex].type

                // result["timestamp"] = Date.now() * 1000 //< Should we set timestamp here?

                root.dialog.testEvent(JSON.stringify(result))
            }
        }
    }

    Connections
    {
        target: root.dialog
        function onEventSent(success, error)
        {
            testButton.enabled = true
            if (success)
                return

            infoDialog.title = "Event test error"
            infoDialogContent.text = error
            infoDialog.show()
        }
    }

    Dialog
    {
        id: infoDialog
        modality: Qt.ApplicationModal
        width: 300

        contentItem: Label
        {
            id: infoDialogContent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: ColorTheme.colors.light1
            font.pixelSize: FontConfig.normal.pixelSize
            wrapMode: Text.Wrap
        }

        buttonBox: DialogButtonBox
        {
            buttonLayout: DialogButtonBox.KdeLayout
            standardButtons: DialogButtonBox.Ok
        }
    }

    Component
    {
        id: comboBoxComponent

        BasicSelectableTableCellDelegate
        {
            tableView: eventPropertiesTableView
            TableView.editDelegate: ComboBox
            {
                anchors.fill: parent
                model: typeToModel[whatsThis] ?? []
                popup.onClosed:
                {
                    eventPropertiesTableView.closeEditor()
                }

                Component.onCompleted:
                {
                    eventPropertiesTableView.editing = true
                    currentIndex = find(JSON.parse(display))
                    popup.open()
                }

                Component.onDestruction:
                {
                    if (currentIndex === -1)
                        return

                    display = JSON.stringify(currentText)
                    eventPropertiesTableView.editing = false
                }
            }
        }
    }

    Component
    {
        id: resourceDelegate

        BasicSelectableTableCellDelegate
        {
            tableView: eventPropertiesTableView
            TableView.editDelegate: ComboBox
            {
                anchors.fill: parent
                textRole: "name"
                valueRole: "value"
                model: typeToModel[whatsThis] ?? []

                popup.onClosed:
                {
                    eventPropertiesTableView.closeEditor()
                }

                Component.onCompleted:
                {
                    eventPropertiesTableView.editing = true
                    currentIndex = indexOfValue(JSON.parse(display))
                    popup.open()
                }

                Component.onDestruction:
                {
                    if (currentIndex === -1)
                        return

                    display = JSON.stringify(currentValue)
                    eventPropertiesTableView.editing = false
                }
            }
        }
    }

    Component
    {
        id: resourcesDelegate

        BasicSelectableTableCellDelegate
        {
            tableView: eventPropertiesTableView
            TableView.editDelegate: MultiSelectionComboBox
            {
                anchors.fill: parent
                textRole: "name"
                model: ListModel {}

                Component.onCompleted:
                {
                    eventPropertiesTableView.editing = true

                    let selectedDevices = JSON.parse(display)
                    for (let i of root.devices)
                    {
                        model.append({
                            "name": i.name,
                            "checked": selectedDevices.includes(i.value),
                            "value": i.value})
                    }

                    Qt.callLater(() => openPopup()) //< Direct `openPopup()` call does not work.
                }

                Component.onDestruction:
                {
                    let selectedDevices = []
                    for (let i = 0; i < model.count; ++i)
                    {
                        const modelItem = model.get(i)
                        if (modelItem.checked)
                            selectedDevices.push(modelItem.value)
                    }

                    display = JSON.stringify(selectedDevices)

                    eventPropertiesTableView.editing = false
                }
            }
        }
    }
}
