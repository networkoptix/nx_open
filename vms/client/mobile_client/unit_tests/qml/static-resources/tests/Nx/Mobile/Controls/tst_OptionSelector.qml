// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.OptionSelector"

    component: OptionSelector {}
    spiedSignals: ["clicked"]

    function test_defaults()
    {
        compare(control.value, undefined)
        compare(control.unselectedValue, undefined)
        compare(control.isDefaultValue, true)
        compare(control.text, "Any")
        compare(control.screenTitle, "Any")
        compare(control.textFieldName, "")
    }

    function test_valueText()
    {
        control.value = "Camera"
        compare(control.text, "Camera")
        compare(control.screenTitle, "Camera")
        compare(control.isDefaultValue, false)
    }

    function test_textFieldName()
    {
        control.textFieldName = "name"
        control.value = {name: "Camera"}
        compare(control.text, "Camera")
    }

    function test_unselectedValue()
    {
        control.unselectedValue = "none"
        control.value = "none"
        compare(control.text, "Any")
        compare(control.isDefaultValue, true)

        control.value = "Camera"
        compare(control.text, "Camera")
        compare(control.isDefaultValue, false)
    }

    function test_unselectedTextValue()
    {
        control.unselectedTextValue = "All"
        compare(control.text, "All")
    }

    function test_valueToTextFunc()
    {
        control.valueToTextFunc = (value) => `<${value}>`
        control.value = "Camera"
        compare(control.text, "<Camera>")
    }

    function test_noValueToTextFunc()
    {
        control.valueToTextFunc = null
        control.value = "Camera"
        compare(control.text, "Camera")
    }

    function test_compareFunc()
    {
        control.compareFunc = (left, right) => left?.id === right?.id
        control.textFieldName = "name"
        control.unselectedValue = {id: 0}
        control.value = {id: 0, name: "Zero"}
        compare(control.isDefaultValue, true)
        compare(control.text, "Any")

        control.value = {id: 1, name: "One"}
        compare(control.isDefaultValue, false)
    }

    function test_intermediateValue()
    {
        control.intermediateValue = "Camera"
        compare(control.isDefaultValue, false)
        compare(control.value, undefined)
    }

    function test_textColor()
    {
        verify(Qt.colorEqual(control.textItem.color, ColorTheme.colors.light10))
        control.value = "Camera"
        verify(Qt.colorEqual(control.textItem.color, ColorTheme.colors.light1))
    }

    function test_descriptionText()
    {
        compare(control.descriptionTextItem.visible, false)
        control.descriptionText = "Description"
        compare(control.descriptionTextItem.visible, true)
    }

    function test_selectorImage()
    {
        verify(!!control.customAreaItem, "Object exists")
        tryCompare(control.customAreaItem, "status", Image.Ready)
    }

    function test_clicked()
    {
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
    }
}
