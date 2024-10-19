// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Mobile.Controls 1.0

import "./helpers.js" as ValueHelpers

/**
 * Basic implementation for the radio group selector screen delegate. Supports text options only.
 */
Column
{
    id: control

    property OptionSelector selector: null

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
            target: control.selector
            property: "textValue"
            value: d.getTextValue(d.selectedIndex)
        }

        Binding
        {
            target: control.selector
            property: "isDefaultValue"
            value: d.selectedIndex === -1
        }
    }
}
