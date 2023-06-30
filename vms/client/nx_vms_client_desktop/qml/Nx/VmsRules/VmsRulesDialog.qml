// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

Dialog
{
    id: root

    property alias rulesTableModel: rulesSortFilterModel.sourceModel
    property alias errorString: alertBar.label.text
    property var dialog: null

    function deleteSelectedRule()
    {
        if (!selectionModel.hasSelection)
            return

        dialog.deleteRule(rulesSortFilterModel.sourceModel.data(
            rulesSortFilterModel.mapToSource(selectionModel.selectedIndexes[0]),
            RulesTableModel.RuleIdRole))
    }

    title: qsTr("Vms Rules")

    width: 1177
    height: 745
    minimumWidth: 800
    minimumHeight: 600
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

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
                text: qsTr("Add Rule")
                iconUrl: "image://svg/skin/buttons/add_20x20_deprecated.svg"
                onClicked:
                {
                    root.dialog.addRule()
                }
            }

            TextButton
            {
                text: qsTr("Duplicate")
                icon.source: "image://svg/skin/text_buttons/copy_20.svg"
                visible: selectionModel.hasSelection

                onClicked:
                {
                    root.dialog.duplicateRule(rulesSortFilterModel.sourceModel.data(
                        rulesSortFilterModel.mapToSource(selectionModel.selectedIndexes[0]),
                        RulesTableModel.RuleIdRole))
                }
            }

            TextButton
            {
                text: qsTr("Delete")
                icon.source: "image://svg/skin/text_buttons/delete_20_deprecated.svg"
                visible: selectionModel.hasSelection

                onClicked:
                {
                    root.deleteSelectedRule()
                }
            }

            Item { Layout.fillWidth: true }

            SearchField
            {
                onTextChanged:
                {
                    tableView.model.setFilterRegularExpression(text)
                }
            }
        }
    }

    contentItem: Item
    {
        Label
        {
            anchors.centerIn: parent
            text: qsTr("No Entries")
            visible: !tableView.visible
        }

        TableView
        {
            id: tableView

            anchors.fill: parent
            anchors.topMargin: header.height
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            columnSpacing: 0
            rowSpacing: 0
            horizontalHeaderVisible: true
            selectionBehavior: TableView.SelectRows
            editTriggers: TableView.NoEditTriggers
            model: RulesSortFilterProxyModel { id: rulesSortFilterModel }
            selectionModel: ItemSelectionModel { id: selectionModel }
            visible: rows !== 0

            columnWidthProvider: function (columnIndex)
            {
                if (columnIndex === RulesTableModel.StateColumn)
                    return 28 // image width + required margin.

                return (width - 28) / (RulesTableModel.ColumnsCount - 1)
            }

            delegate: Rectangle
            {
                id: cellDelegate

                required property bool selected
                required property bool current

                implicitHeight: 28
                color: selected ? ColorTheme.colors.brand_d6 : ColorTheme.colors.dark7

                RowLayout
                {
                    id: cellDelegateLayout
                    anchors.fill: parent
                    spacing: 8

                    Image
                    {
                        Layout.alignment: Qt.AlignVCenter

                        sourceSize.width: 20
                        sourceSize.height: 20

                        source: decoration ?? ""
                        visible: !!decoration
                    }

                    Text
                    {
                        id: displayText

                        Layout.fillWidth: true
                        Layout.rightMargin: 8

                        text: display ? display : ""
                        elide: Text.ElideRight
                        color: ColorTheme.text
                        visible: !!display
                    }
                }

                MouseArea
                {
                    anchors.fill: parent
                    onClicked:
                    {
                        selectionModel.select(
                            tableView.index(row, column),
                            ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
                        tableView.focus = true
                    }

                    onDoubleClicked:
                    {
                        root.dialog.editRule(ruleId)
                    }
                }
            }

            Keys.onPressed: (event) =>
            {
                if (!selectionModel.hasSelection)
                    return

                if (event.key == Qt.Key_Delete
                    || (Qt.platform.os === "osx" && event.key == Qt.Key_Backspace))
                {
                    root.deleteSelectedRule()
                    event.accepted = true;
                }
                else if ((event.key == Qt.Key_Down || event.key == Qt.Key_Up)
                    && selectionModel.hasSelection)
                {
                    const rowToSelect = MathUtils.bound(
                        0,
                        selectionModel.selectedIndexes[0].row + (event.key == Qt.Key_Down ? 1 : -1),
                        tableView.rows - 1)
                    selectionModel.select(
                        tableView.index(rowToSelect, 0),
                        ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)

                    event.accepted = true;
                }
            }
        }
    }

    AlertBar
    {
        id: alertBar
        anchors.bottom: buttonBox.top
        label.text: ""
        visible: !!label.text
    }

    Control
    {
        id: footer
        anchors.verticalCenter: buttonBox.verticalCenter
        padding: 16
        contentItem: Row
        {
            spacing: 8

            TextButton
            {
                text: qsTr("Event Log...")
                icon.source: "image://svg/skin/buttons/event_log_20_deprecated.svg"
            }

            TextButton
            {
                text: qsTr("Reset To Defaults...")
                icon.source: "image://svg/skin/text_buttons/reload_20.svg"

                onClicked:
                {
                    root.dialog.resetToDefaults()
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok
    }
}
