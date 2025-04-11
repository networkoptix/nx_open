// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

Dialog
{
    id: root

    required property KeyValueModel headersModel

    readonly property var kTextWithoutSpaceRegExp: /^\S+$/
    readonly property var kAnyTextRegExp: /.*/

    minimumWidth: 450
    minimumHeight: 425
    title: qsTr("Custom HTTP headers")

    acceptShortcutEnabled: !headersTableView.editing
    rejectShortcutEnabled: !headersTableView.editing

    modality: Qt.ApplicationModal

    Control
    {
        id: header
        width: root.width
        padding: 16

        contentItem: RowLayout
        {
            spacing: 8

            Button
            {
                text: qsTr("Add")
                icon.source: "image://skin/20x20/Outline/add.svg"
                visible: !headersTableView.hasCheckedRows
                enabled: !headersTableView.editing
                onClicked:
                {
                    addHeaderDialog.open()
                }
            }

            Button
            {
                text: qsTr("Delete")
                icon.source: "image://skin/20x20/Outline/delete.svg"
                visible: headersTableView.hasCheckedRows
                enabled: !headersTableView.editing
                onClicked:
                {
                    headersTableView.removeCheckedRows()
                }
            }
        }
    }

    contentItem: CheckableTableView
    {
        id: headersTableView
        anchors.fill: parent
        anchors.topMargin: header.height
        anchors.bottomMargin: buttonBox.height
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        horizontalHeaderVisible: true
        selectionBehavior: TableView.SelectRows
        sourceModel: root.headersModel
        editTriggers: TableView.SingleTapped
        delegate: DelegateChooser
        {
            DelegateChoice
            {
                column: 0
                BasicSelectableTableCheckableCellDelegate
                {
                    enabled: !TableView.view.editing
                }
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
                        validator: RegularExpressionValidator
                        {
                            regularExpression: column === 1 ? kTextWithoutSpaceRegExp : kAnyTextRegExp
                        }

                        Component.onCompleted:
                        {
                            TableView.view.editing = true
                            selectAll()
                        }

                        Component.onDestruction:
                        {
                            edit = text
                            TableView.view.editing = false
                        }
                    }
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        onClicked:
        {
            headersTableView.closeEditor()
        }
    }

    ModalDialog
    {
        id: addHeaderDialog

        function open()
        {
            keyTextField.text = ""
            valueTextField.text = ""
            openNewIn(root)
        }

        contentItem: GridLayout
        {
            columns: 2
            columnSpacing: 8
            rowSpacing: 8

            Label
            {
                text: qsTr("Key")
            }

            TextField
            {
                id: keyTextField
                Layout.fillWidth: true
                validator: RegularExpressionValidator
                {
                    regularExpression: kTextWithoutSpaceRegExp
                }
            }

            Label
            {
                text: qsTr("Value")
            }

            TextField
            {
                id: valueTextField
                Layout.fillWidth: true
            }
        }

        onAccepted:
        {
            root.headersModel.append(keyTextField.text, valueTextField.text)
        }

        buttonBox: DialogButtonBox
        {
            buttonLayout: DialogButtonBox.KdeLayout
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        }

        Component.onCompleted:
        {
            // Do not allow to save with empty header key.
            buttonBox.standardButton(DialogButtonBox.Ok).enabled =
                Qt.binding(() => { return keyTextField.text })
        }
    }
}
