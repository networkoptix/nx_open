// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Mobile.Controls

import "./helpers.js" as ValueHelpers

/**
 * Basic implementation for the radio group selector screen delegate. Supports text options only.
 */
Column
{
    id: control

    required property OptionSelector selector

    property alias model: repeater.model

    property string valueRoleId //< Role name for the value to be extracted from the model.

    spacing: 4

    signal applyRequested()

    function setValue(newValue)
    {
        d.selectedIndex = ValueHelpers.getIndex(newValue, model, selector.compareFunc)
    }

    function clear()
    {
        d.selectedIndex = -1
    }

    Repeater
    {
        id: repeater

        delegate: StyledRadioButton
        {
            id: item

            height: 56
            width: parent.width
            backgroundRadius: 8
            checkedBackgroundColor: ColorTheme.colors.dark8
            text: d.getTextValue(index)
            checked: d.selectedIndex === index
            onCheckedChanged:
            {
                if (!checked)
                    return

                d.selectedIndex = index
                control.applyRequested()
            }
        }
    }

    NxObject
    {
        id: d

        property int selectedIndex: getCurrentIndex()

        function getCurrentIndex()
        {
            return control.selector
                ? ValueHelpers.getIndex(control.selector.value, control.model, selector.compareFunc)
                : -1
        }

        function getTextValue(index)
        {
            if (!control.selector)
                return ""

            return ValueHelpers.getTextValue(index,
                control.model,
                control.selector.unselectedValue,
                control.selector.unselectedTextValue,
                control.valueRoleId,
                control.selector.textFieldName,
                control.selector.valueToTextFunc)
        }

        Connections
        {
            target: control.selector

            function onValueChanged()
            {
                const newIndex = d.getCurrentIndex()
                if (newIndex !== d.selectedIndex)
                    d.selectedIndex = newIndex
            }
        }

        Binding
        {
            restoreMode: Binding.RestoreNone
            target: control.selector
            property: "value"
            value: ValueHelpers.getValue(
                d.selectedIndex,
                control.model,
                control.selector.unselectedValue,
                control.valueRoleId)
        }

        Binding
        {
            restoreMode: Binding.RestoreNone
            target: control.selector
            property: "text"
            value: d.getTextValue(d.selectedIndex)
        }

        Binding
        {
            restoreMode: Binding.RestoreNone
            target: control.selector
            property: "isDefaultValue"
            value: d.selectedIndex === -1
        }
    }
}
