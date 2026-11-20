// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.TextArea"

    component: TextArea { width: 268; height: 100 }
    spiedSignals: ["textChanged"]

    readonly property string longText: "Line\n".repeat(20)

    function test_defaults()
    {
        compare(control.text, "")
        compare(control.labelText, "")
        compare(control.readOnly, false)
        compare(control.backgroundMode, FieldBackground.Mode.Dark)
    }

    function test_text_data()
    {
        return [
            {tag: "characters", text: "Alice"},
            {tag: "numbers", text: "12345"},
            {tag: "special characters", text: "Name & Co. (1985) <b>#1</b> 100%"},
            {tag: "several lines", text: "First\nSecond\nThird"},
        ]
    }

    function test_text(data)
    {
        const textArea = findById("textArea")
        textArea.focus = true
        control.text = data.text
        compare(control.text, data.text)
        compare(textArea.text, data.text)
    }

    function test_textChanged()
    {
        findById("textArea").text = "Typed"
        compare(control.text, "Typed")
        compare(signalSpy("textChanged").count, 1)
    }

    function test_labelText()
    {
        control.labelText = "Description"
        compare(control.labelText, "Description")
        compare(findById("fieldBackground").labelText, "Description")
    }

    function test_readOnly()
    {
        control.readOnly = true
        compare(control.readOnly, true)
        compare(findById("textArea").readOnly, true)
    }

    function test_backgroundMode()
    {
        control.backgroundMode = FieldBackground.Mode.Light
        compare(control.backgroundMode, FieldBackground.Mode.Light)
        compare(findById("fieldBackground").mode, FieldBackground.Mode.Light)
    }

    function test_labelIsCompactWithText()
    {
        const fieldBackground = findById("fieldBackground")
        compare(fieldBackground.compactLabelMode, false)
        control.text = "Text"
        compare(fieldBackground.compactLabelMode, true)
    }

    function test_clickGivesActiveFocus()
    {
        const textArea = findById("textArea")
        compare(textArea.activeFocus, false)
        mouseClick(control)
        tryCompare(textArea, "activeFocus", true)
        tryCompare(findById("fieldBackground"), "compactLabelMode", true)
    }

    function test_textColorIsDimmedWhenDisabled()
    {
        const textArea = findById("textArea")
        compare(textArea.color.a, 1)
        control.enabled = false
        fuzzyCompare(textArea.color.a, 0.3, 0.01)
    }

    function test_implicitHeightFollowsText()
    {
        const textArea = findById("textArea")
        compare(control.implicitHeight, textArea.implicitHeight)
        const emptyHeight = control.implicitHeight
        control.text = "First\nSecond"
        verify(control.implicitHeight > emptyHeight)
        compare(control.implicitHeight, textArea.implicitHeight)
    }

    function test_implicitHeightIsLimited()
    {
        control.text = longText
        verify(findById("textArea").implicitHeight > 150)
        compare(control.implicitHeight, 150)
    }

    function test_viewFollowsCursor()
    {
        const textArea = findById("textArea")
        control.text = longText
        verify(control.contentHeight > control.height)

        textArea.cursorPosition = textArea.length
        compare(control.contentY, control.contentHeight - control.height)

        textArea.cursorPosition = 0
        compare(control.contentY, 0)
    }

    function test_createdWithTextIsAtBeginning()
    {
        const area = createTemporaryObject(component, testCase, {text: longText})
        verify(!!area, "Component exists")
        verify(area.contentHeight > area.height)

        // The view returns to the bounds with an animation.
        tryCompare(area, "contentY", 0)
    }
}
