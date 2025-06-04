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

    property alias rulesTableModel: rulesSortFilterModel.sourceModel
    property var dialog: null
    property alias filterText: searchField.text
    property bool developerMode: false

    function deleteCheckedRules()
    {
        if (!tableView.hasCheckedRows)
            return

        dialog.deleteRules(rulesSortFilterModel.getRuleIds(tableView.checkedRows))
    }

    function updateEnableAndDisableButtonsVisibility()
    {
        if (!tableView.hasCheckedRows)
        {
            enableButton.visible = false
            disableButton.visible = false

            return
        }

        const checkState = rulesSortFilterModel.getRuleCheckStates(tableView.checkedRows)
        switch (checkState)
        {
            case Qt.Unchecked: //< All the rules disabled.
                enableButton.visible = true
                disableButton.visible = false
                break
            case Qt.Checked: //< All the selected rules enabled.
                enableButton.visible = false
                disableButton.visible = true
                break
            case Qt.PartiallyChecked:
                enableButton.visible = true
                disableButton.visible = true
                break
            default:
                enableButton.visible = false
                disableButton.visible = false
                break;
        }
    }

    title: qsTr("Event Rules")

    width: 1177
    height: 745
    minimumWidth: 800
    minimumHeight: 600
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    modality: Qt.NonModal

    acceptShortcutEnabled: !tableView.selectionModel.hasSelection

    onVisibleChanged: tableView.checkedRows = []

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
                icon.source: "image://skin/20x20/Outline/add.svg"
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
                color: ColorTheme.colors.light1
                font.pixelSize: FontConfig.large.pixelSize
            }

            TextButton
            {
                text: qsTr("Schedule")
                icon.source: "image://skin/20x20/Outline/calendar.svg"
                visible: tableView.hasCheckedRows

                onClicked:
                {
                    const ruleIds = rulesSortFilterModel.getRuleIds(tableView.checkedRows)
                    root.dialog.editSchedule(ruleIds)
                }
            }

            TextButton
            {
                id: enableButton
                text: qsTr("Enable")
                icon.source: "image://skin/20x20/Outline/checkmark.svg"
                visible: false

                onClicked:
                {
                    root.dialog.setRulesState(
                        rulesSortFilterModel.getRuleIds(tableView.checkedRows), true)
                }
            }

            TextButton
            {
                id: disableButton
                text: qsTr("Disable")
                icon.source: "image://skin/20x20/Outline/disable.svg"
                visible: false

                onClicked:
                {
                    root.dialog.setRulesState(
                        rulesSortFilterModel.getRuleIds(tableView.checkedRows), false)
                }
            }

            TextButton
            {
                text: qsTr("Duplicate")
                icon.source: "image://skin/20x20/Outline/copy.svg"
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
                icon.source: "image://skin/20x20/Outline/delete.svg"
                visible: tableView.hasCheckedRows

                onClicked:
                {
                    root.deleteCheckedRules()
                }
            }

            Item { Layout.fillWidth: true }

            SearchField
            {
                id: searchField

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
            horizontalHeaderView.resizableColumns: false
            selectionBehavior: TableView.SelectRows
            sourceModel: RulesSortFilterProxyModel
            {
                id: rulesSortFilterModel
            }
            visible: rows !== 0
            columnWidthProvider: function(column)
            {
                if (!isColumnLoaded(column))
                    return -1

                const checkableColumnWidth = Math.max(
                    implicitColumnWidth(0), horizontalHeaderView.implicitColumnWidth(0))
                if (column === 0)
                    return checkableColumnWidth

                const stateColumnWidth = Math.max(
                    implicitColumnWidth(0), horizontalHeaderView.implicitColumnWidth(0))
                if (column === 1)
                    return stateColumnWidth

                return (width - checkableColumnWidth - stateColumnWidth) / (columns - 2)
            }

            delegate: DelegateChooser
            {
                id: delegateChooser

                DelegateChoice
                {
                    column: 0
                    BasicSelectableTableCheckableCellDelegate {}
                }

                DelegateChoice
                {
                    BasicSelectableTableCellDelegate
                    {
                        onClicked:
                        {
                            root.dialog.editRule(tableView.model.data(
                                tableView.model.index(row, column), RulesTableModel.RuleIdRole))
                        }
                    }
                }
            }

            deleteShortcut.onActivated: root.deleteCheckedRules()
            enterShortcut.onActivated:
            {
                root.dialog.editRule(tableView.model.data(
                    tableView.selectionModel.selectedIndexes[0], RulesTableModel.RuleIdRole))
            }

            Component.onCompleted:
            {
                forceLayout()
            }
        }
    }

    Column
    {
        id: alertsColumn
        anchors.bottom: buttonBox.top
        spacing: 2

        Repeater
        {
            model: ListModel
            {
                id: errorsListModel
            }

            delegate: DialogBanner
            {
                width: root.width
                text: model.text
                style: model.style
                visible: true
                closeable: true

                onCloseClicked:
                {
                    errorsListModel.remove(index)
                }
            }
        }
    }

    Connections
    {
        target: tableView
        function onCheckedRowsChanged()
        {
            root.updateEnableAndDisableButtonsVisibility()
        }
    }

    Connections
    {
        target: rulesSortFilterModel
        enabled: tableView.checkedRows.length > 0
        function onDataChanged(topLeft, bottomRight, roles)
        {
            root.updateEnableAndDisableButtonsVisibility()
        }
    }

    Connections
    {
        target: root.dialog
        function onError(error)
        {
            errorsListModel.append({"text": error, "style": DialogBanner.Style.Error})
        }

        function onWarning(warning)
        {
            errorsListModel.append({"text": warning, "style": DialogBanner.Style.Warning})
        }
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
                icon.source: "image://skin/20x20/Outline/event_log.svg"

                onClicked:
                {
                    root.dialog.openEventLogDialog()
                }
            }

            TextButton
            {
                text: qsTr("Reset To Defaults...")
                icon.source: "image://skin/20x20/Outline/reload.svg"

                onClicked:
                {
                    root.dialog.resetToDefaults()
                }
            }

            TextButton
            {
                text: qsTr("Test events...")
                visible: developerMode

                onClicked:
                {
                    root.dialog.openTestEventDialog()
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
