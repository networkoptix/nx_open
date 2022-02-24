// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

CollapsiblePanel
{
    id: control

    required property RightPanelModel model

    property var selectedValue

    readonly property alias count: comboBox.count

    name: qsTr("Event Type")
    value: comboBox.currentText

    onClearRequested:
        selectedValue = undefined

    onSelectedValueChanged:
    {
        comboBox.currentIndex = selectedValue !== undefined
            ? comboBox.indexOfValue(selectedValue)
            : -1
    }

    contentItem: TreeComboBox
    {
        id: comboBox

        treeModel: control.model ? control.model.analyticsEvents : null
        textRole: "name"
        valueRole: "id"
        enabledRole: "enabled"
        displayText: currentValue !== undefined ? currentText : qsTr("All")

        onActivated:
            control.selectedValue = currentValue
    }
}
