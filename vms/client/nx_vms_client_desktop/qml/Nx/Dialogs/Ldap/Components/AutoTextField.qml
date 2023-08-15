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

    Item
    {
        Layout.alignment: Qt.AlignTop

        implicitWidth: autoCheckBox.implicitWidth
        implicitHeight: textField.textField.implicitHeight

        CheckBox
        {
            id: autoCheckBox

            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 3

            text: qsTr("Auto")

            // Focus the textField when user unchecks this checkbox.
            onCheckedChanged: if (!checked && visible) textField.forceActiveFocus()
        }
    }
}
