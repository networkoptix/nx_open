// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import Qt.labs.qmlmodels

import Nx
import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.core
import nx.vms.client.desktop

Dialog
{
    id: root

    property alias rulesTableModel: rulesSortFilterModel.sourceModel
    property alias errorString: alertBar.label.text
    property var dialog: null

    function deleteCheckedRules()
    {
        if (!tableView.hasCheckedRows)
            return

        dialog.deleteRules(rulesSortFilterModel.getRuleIds(tableView.checkedRows))
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
                visible: !tableView.hasCheckedRows
                onClicked:
                {
                    root.dialog.addRule()
                }
            }

            Text
            {
                visible: tableView.hasCheckedRows
                text: qsTr("%1 selected:").arg(tableView.checkedRows.length)
                color: ColorTheme.brightText
                font.pixelSize: FontConfig.large.pixelSize
            }

            TextButton
            {
                text: qsTr("Schedule")
                icon.source: "image://svg/skin/text_buttons/calendar_20.svg"
                visible: tableView.hasCheckedRows

                onClicked:
                {
                    const ruleIds = rulesSortFilterModel.getRuleIds(tableView.checkedRows)
                    root.dialog.editSchedule(ruleIds)
                }
            }

            TextButton
            {
                text: qsTr("Duplicate")
                icon.source: "image://svg/skin/text_buttons/copy_20.svg"
                visible: tableView.checkedRows.length === 1

                onClicked:
                {
                    const ruleIds = rulesSortFilterModel.getRuleIds(tableView.checkedRows)
                    root.dialog.duplicateRule(ruleIds[0])
                }
            }

            TextButton
            {
                text: qsTr("Delete")
                icon.source: "image://svg/skin/text_buttons/delete_20_deprecated.svg"
                visible: tableView.hasCheckedRows

                onClicked:
                {
                    root.deleteCheckedRules()
                }
            }

            Item { Layout.fillWidth: true }

            SearchField
            {
                onTextChanged:
                {
                    rulesSortFilterModel.setFilterRegularExpression(text)
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

        CheckableTableView
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
            sourceModel: RulesSortFilterProxyModel
            {
                id: rulesSortFilterModel
            }
            visible: rows !== 0

            delegate: DelegateChooser
            {
                DelegateChoice
                {
                    column: 0
                    BasicSelectableTableCheckableCellDelegate {}
                }

                DelegateChoice
                {
                    BasicSelectableTableCellDelegate
                    {
                        onDoubleClicked:
                        {
                            root.dialog.editRule(rulesSortFilterModel.data(
                                rulesSortFilterModel.index(row, column), RulesTableModel.RuleIdRole))
                        }
                    }
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

                onClicked:
                {
                    root.dialog.openEventLogDialog()
                }
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
