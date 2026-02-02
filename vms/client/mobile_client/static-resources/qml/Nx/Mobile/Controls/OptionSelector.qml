// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Ui

import "private"

/**
 * Implements basic functionaity for the selectors which oppen separate screen with content to
 * select the value.
 */
BaseOption
{
    id: control

    property Item openedScreen: null

    property Component visualDelegate //< Visual delegate for the selector control.
    property Component screenDelegate //< Visual delegate for the option selector screen.

    // Name of field for the value which holds the text representation.
    property string textFieldName: ""

    // Calue-to-text transformation functor. Can be replaced for the custom one.
    property var valueToTextFunc: (value) => d.defaultValueToText(value)

    // Compare functor. Can be replaced for the custom one
    property var compareFunc: (left, right) => CoreUtils.equalValues(left, right)

    property var unselectedValue: undefined //< Specific unselected value
    property var unselectedTextValue: qsTr("Any") //< Text representation of the unselected value.

    property bool isDefaultValue: true //< Specifies if current intermediate value is a default one.
    property var value: unselectedValue //< Current value.

    property var intermediateValue: value //< Used to determine if option has some default value.

    property string screenTitle: text //< Title of the value selection screen.

    Binding
    {
        target: control
        property: "isDefaultValue"
        value: control.compareFunc(control.intermediateValue, control.unselectedValue)
            || control.intermediateValue === undefined
    }

    Binding
    {
        target: control
        property: "text"
        value: control.valueToTextFunc
            ? control.valueToTextFunc(control.value)
            : d.defaultValueToText(control.value)
    }

    function setValue(newValue)
    {
        const delegate = openedScreen && openedScreen.delegateItem
        if (delegate && delegate.hasOwnProperty("setValue"))
        {
            if (delegate.value !== newValue)
                delegate.setValue(newValue)
        }
        else if (compareFunc(newValue, value))
        {
            value = newValue
        }
    }

    customArea: Image
    {
        id: selectorImage

        anchors.verticalCenter: parent.verticalCenter
        source: lp("/images/open_selector.svg")
        width: 24
        height: 24
        visible: !LayoutController.isTabletLayout
    }

    NxObject
    {
        id: d

        function defaultValueToText(value)
        {
            if (control.compareFunc(value, control.unselectedValue) || !value)
                return control.unselectedTextValue

            return control.textFieldName
                ? value[control.textFieldName]
                : value
        }
    }
}
