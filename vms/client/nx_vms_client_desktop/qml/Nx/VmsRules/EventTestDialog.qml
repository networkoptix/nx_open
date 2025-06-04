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
    property var events: []

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

                const eventProperties = root.dialog.getEventProperties(model[currentIndex].type)
                const eventPropertyMetatypes = root.dialog.getEventPropertyMetatypes(model[currentIndex].type)
                for (let propertyName of Object.keys(eventProperties))
                {
                    const row = {
                        "name": propertyName,
                        "value": JSON.stringify(eventProperties[propertyName]),
                        "type": eventPropertyMetatypes[propertyName]}

                    eventPropertiesModel.appendRow(row)
                }
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
                TableModelColumn { display: "name" }
                TableModelColumn { display: "value"}
            }

            delegate: DelegateChooser
            {
                DelegateChoice
                {
                    column: 0
                    BasicSelectableTableCellDelegate{}
                }
                DelegateChoice
                {
                    BasicSelectableTableCellDelegate
                    {
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

                        Component.onCompleted:
                        {
                            // This workaround is caused by the qt bug described here
                            // https://bugreports.qt.io/browse/QTBUG-136658
                            tableView = eventPropertiesTableView
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

                var result = {}
                for (let i = 0; i < eventPropertiesModel.rowCount; ++i)
                {
                    const row = eventPropertiesModel.rows[i]

                    result[row.name] = JSON.parse(row.value)
                }

                result["type"] = root.events[eventsListView.currentIndex].type

                // result["timestamp"] = Date.now() * 1000 //< Should we set timestamp here?

                root.dialog.testEvent(JSON.stringify(result))

                enabled = false
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
}
