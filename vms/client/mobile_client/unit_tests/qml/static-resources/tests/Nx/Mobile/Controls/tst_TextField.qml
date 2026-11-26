// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.TextField"

    component: TextField
    {
        width: 300
        labelText: "Label"
    }

    function test_labelMovesUpWithText()
    {
        const label = findById("label")
        compare(label.y, 18)
        compare(label.font.pixelSize, 16)

        control.text = "Text"
        compare(label.y, 8)
        compare(label.font.pixelSize, 12)
    }

    function test_labelMovesUpWithFocus()
    {
        const label = findById("label")
        control.focus = true
        compare(label.y, 8)
        compare(label.font.pixelSize, 12)

        control.focus = false
        compare(label.y, 18)
        compare(label.font.pixelSize, 16)
    }

    function test_focusHighlight()
    {
        const underline = findById("underline")
        const mainBackground = findById("mainBackground")
        compare(underline.visible, false)
        verify(Qt.colorEqual(mainBackground.color, ColorTheme.colors.dark7))

        control.focus = true
        compare(underline.visible, true)
        verify(Qt.colorEqual(mainBackground.color, ColorTheme.colors.dark8))

        control.focus = false
        compare(underline.visible, false)
        verify(Qt.colorEqual(mainBackground.color, ColorTheme.colors.dark7))
    }

    function test_clearButtonVisibility()
    {
        const clearButton = findById("clearButton")
        compare(clearButton.visible, false)
        compare(control.rightPadding, 16)

        control.text = "Text"
        compare(clearButton.visible, true)
        compare(control.rightPadding, clearButton.width)
    }

    function test_clearButton()
    {
        const clearButton = findById("clearButton")
        control.text = "Text"
        mouseClick(clearButton)
        tryCompare(control, "text", "")
        tryCompare(control, "activeFocus", true)
        tryCompare(clearButton, "visible", false)
    }
}
