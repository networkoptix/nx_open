// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls

import "../../UserManagement/Components"

RowLayout
{
    id: control

    property alias text: textField.text
    property alias auto: autoCheckBox.checked
    readonly property var textField: textField
    readonly property alias checkBoxWidth: autoCheckBox.implicitWidth

    baselineOffset: textField.y + textField.baselineOffset

    function validate() { return textField.validate() }

    TextFieldWithValidator
    {
        id: textField

        Layout.fillWidth: true
        focus: true
        enabled: !autoCheckBox.checked

        property string savedValue: ""
        onEnabledChanged:
        {
            if (!enabled)
            {
                savedValue = text
                text = ""
                warningState = false
            }
            else
            {
                text = savedValue
                savedValue = ""
            }
        }

        validateFunc: (text) =>
        {
            return !autoCheckBox.checked && !text
                ? qsTr("This field cannot be empty")
                : ""
        }
    }

    CheckBox
    {
        id: autoCheckBox

        Layout.alignment: Qt.AlignBaseline

        text: qsTr("Auto")

        // Focus the textField when user unchecks this checkbox.
        onCheckedChanged:
        {
            if (!checked && visible)
                textField.forceActiveFocus()
        }
    }
}
