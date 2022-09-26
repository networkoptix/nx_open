// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

ActiveTextItem
{
    id: control

    property string defaultValue: ""
    property string validationRegex: ""
    property string validationRegexFlags: ""
    property string validationErrorMessage: ""

    property alias textFieldItem: control.contentItem
    readonly property bool filled: textFieldItem.text !== ""

    signal valueChanged()

    Connections
    {
        target: textFieldItem
        function onTextChanged() { control.valueChanged() }
    }

    Binding { target: textFieldItem; property: "text"; value: defaultValue }

    Binding
    {
        target: textFieldItem
        property: "warningText"
        value: errorMessage || validationErrorMessage
    }

    Binding
    {
        target: textFieldItem
        property: "validationRegex"
        value: new RegExp(validationRegex, validationRegexFlags)
    }

    function getValue()
    {
        return textFieldItem.text
    }

    function setValue(value)
    {
        textFieldItem.text = value
        textFieldItem.updateWarning()
    }

    function setErrorMessage(value)
    {
        errorMessage = value
        textFieldItem.updateWarning()
    }
}
