// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Controls

import Nx
import Nx.Controls
import Nx.Core
import Nx.Models

import nx.vms.client.core
import nx.vms.client.desktop

Button
{
    id: control

    property RowCheckModel model //< The property is not marked as required intentionally, to prevent assignment from the wrong context.

    property var headerDataAccessor: ModelDataAccessor
    {
        function updateCheckState()
        {
            let hasCheckedValues = false
            let hasUncheckedValues = false

            // TODO: #mmalofeev move to c++ model.
            for (let row = 0; row < control.model.rowCount(); ++row)
            {
                const val = headerDataAccessor.getData(
                    control.model.index(row, index),
                    "checkState")

                if (val === Qt.Checked)
                    hasCheckedValues = true
                if (val === Qt.Unchecked)
                    hasUncheckedValues = true

                if (hasUncheckedValues && hasCheckedValues)
                    break
            }

            if (hasCheckedValues && hasUncheckedValues)
                checkBox.checkState = Qt.PartiallyChecked
            else if (hasCheckedValues)
                checkBox.checkState = Qt.Checked
            else if (hasUncheckedValues)
                checkBox.checkState = Qt.Unchecked
            else if (control.model.rowCount() === 0)
                checkBox.checkState = Qt.Unchecked
            else
                console.warn("Unexpected model check states")
        }

        function syncCheckState(checkState)
        {
            if (checkState === Qt.PartiallyChecked)
                return

            for (let row = 0; row < control.model.rowCount(); ++row)
                headerDataAccessor.setData(row, index, checkState, "checkState")
        }

        model: control.model

        onHeaderDataChanged: (orientation, first, last) =>
        {
            if (index < first || index > last)
                return

            if (orientation !== Qt.Horizontal)
                return

            control.text = model.headerData(index, Qt.Horizontal)
        }

        onDataChanged: updateCheckState()
        onRowsRemoved: updateCheckState()
        onRowsInserted: updateCheckState()

        Component.onCompleted: updateCheckState()
    }

    leftPadding: 8
    rightPadding: 8
    clip: true
    backgroundColor: "transparent"
    hoveredColor: ColorTheme.transparent(ColorTheme.colors.dark9, 0.2)
    pressedColor: hoveredColor

    textColor: ColorTheme.colors.light4
    font.pixelSize: 14
    font.weight: Font.Medium

    contentItem: Item
    {
        implicitWidth: checkBox.implicitWidth
        implicitHeight: checkBox.implicitHeight

        CheckBox
        {
            id: checkBox

            font: control.font
            anchors.verticalCenter: parent.verticalCenter

            onToggled:
            {
                headerDataAccessor.syncCheckState(checkState)
            }
        }
    }
}
