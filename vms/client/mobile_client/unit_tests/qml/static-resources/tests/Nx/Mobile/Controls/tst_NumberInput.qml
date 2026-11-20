// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.NumberInput"

    component: NumberInput { width: 300; height: 56 }
    spiedSignals: ["applyRequested"]

    function test_implicitSize()
    {
        verify(control.implicitHeight > 0)
        compare(control.implicitHeight, findById("textField").height)
    }

    function test_defaultValue()
    {
        compare(control.value, undefined)
        compare(findById("textField").text, "")
    }

    function test_defaultMode()
    {
        compare(control.mode, NumberInput.Integer)
    }

    function test_labelText()
    {
        control.labelText = "Port"
        compare(control.labelText, "Port")
        compare(findById("textField").labelText, "Port")
    }

    function test_valueIsShownAsText()
    {
        control.value = 42
        compare(findById("textField").text, "42")
    }

    function test_undefinedValueClearsText()
    {
        control.value = 42
        control.value = undefined
        compare(findById("textField").text, "")
    }

    function test_commitInteger()
    {
        const textField = findById("textField")
        textField.focus = true
        textField.text = "42"
        control.commit()
        compare(control.value, 42)
    }

    function test_commitReal()
    {
        const textField = findById("textField")
        control.mode = NumberInput.Real
        textField.focus = true
        textField.text = "1.5"
        control.commit()
        compare(control.value, 1.5)
    }

    function test_commitEmptyText()
    {
        const textField = findById("textField")
        control.value = 42
        textField.focus = true
        textField.text = ""
        control.commit()
        compare(control.value, undefined)
    }

    function test_commitBoundsToMinimum()
    {
        const textField = findById("textField")
        control.minimum = 10
        textField.focus = true
        textField.text = "5"
        control.commit()
        compare(control.value, 10)
        compare(textField.text, "10")
    }

    function test_commitBoundsToMaximum()
    {
        const textField = findById("textField")
        control.maximum = 100
        textField.focus = true
        textField.text = "500"
        control.commit()
        compare(control.value, 100)
        compare(textField.text, "100")
    }

    function test_commitOnFocusLoss()
    {
        const textField = findById("textField")
        textField.forceActiveFocus()
        verify(textField.activeFocus)
        textField.text = "42"
        textField.focus = false
        compare(control.value, 42)
    }

    function test_selectAll()
    {
        const textField = findById("textField")
        control.value = 42
        control.selectAll()
        compare(textField.selectedText, "42")
    }

    function test_clear()
    {
        const textField = findById("textField")
        control.value = 42
        control.clear()
        compare(textField.text, "")
    }

    function test_acceptsNumbers()
    {
        const textField = findById("textField")
        textField.focus = true
        textField.text = "12345"
        verify(textField.acceptableInput)
    }

    function test_rejectsCharacters()
    {
        const textField = findById("textField")
        textField.focus = true
        textField.text = "abc"
        verify(!textField.acceptableInput)
    }

    function test_rejectsSpecialCharacters()
    {
        const textField = findById("textField")
        textField.focus = true
        textField.text = "1&2"
        verify(!textField.acceptableInput)
    }

    function test_rejectsNumberOutOfRange()
    {
        const textField = findById("textField")
        control.minimum = 10
        control.maximum = 20
        textField.focus = true
        textField.text = "15"
        verify(textField.acceptableInput)
        textField.text = "25"
        verify(!textField.acceptableInput)
    }

    function test_activeFocusIsPassedToTextField()
    {
        control.forceActiveFocus()
        verify(findById("textField").activeFocus)
    }

    function test_applyRequestedOnReturn()
    {
        const textField = findById("textField")
        textField.forceActiveFocus()
        verify(textField.activeFocus)
        textField.text = "42"
        keyClick(Qt.Key_Return)
        tryCompare(signalSpy("applyRequested"), "count", 1)
        compare(control.value, 42)
    }

    function test_applyRequestedOnEnter()
    {
        const textField = findById("textField")
        textField.forceActiveFocus()
        verify(textField.activeFocus)
        textField.text = "42"
        keyClick(Qt.Key_Enter)
        tryCompare(signalSpy("applyRequested"), "count", 1)
        compare(control.value, 42)
    }
}
