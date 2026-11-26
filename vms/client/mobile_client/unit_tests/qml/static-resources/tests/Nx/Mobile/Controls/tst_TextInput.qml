// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.TextInput"

    component: TextInput { labelText: "Label" }

    function test_errorTextIsClearedByEditing()
    {
        control.errorText = "Error"
        control.text = "Text"
        compare(control.errorText, "")
    }

    function test_bottomTextGrowsImplicitHeight()
    {
        compare(control.implicitHeight, 56)
        compare(control.bottomInset, 0)

        control.supportText = "Support"
        verify(control.background.bottomTextHeight > 0)
        compare(control.implicitHeight, 56 + control.background.bottomTextHeight)
        compare(control.bottomInset, control.background.bottomTextHeight)
    }

    function test_labelIsCompactWithText()
    {
        compare(control.background.compactLabelMode, false)
        control.text = "Text"
        compare(control.background.compactLabelMode, true)
    }

    function test_disabledColor()
    {
        verify(Qt.colorEqual(control.color, ColorTheme.colors.light4))
        control.enabled = false
        verify(Qt.colorEqual(control.color, ColorTheme.transparent(ColorTheme.colors.light4, 0.3)))
    }

    function test_actionButtonVisibility()
    {
        compare(control.showActionButton, false)
        compare(control.rightPadding, 12)

        control.text = "Text"
        compare(control.showActionButton, false)

        control.focus = true
        compare(control.showActionButton, true)
        compare(control.rightPadding, 44)

        control.text = ""
        compare(control.showActionButton, false)
        compare(control.rightPadding, 12)
    }

    function test_actionButtonClearsText()
    {
        control.focus = true
        control.text = "Text"
        control.errorText = "Error"
        mouseClick(control.actionButtonImage)
        tryCompare(control, "text", "")
        tryCompare(control, "errorText", "")
        tryCompare(control, "activeFocus", true)
    }

    function test_customActionButtonAction()
    {
        control.focus = true
        control.text = "Text"
        control.actionButtonAction = () => { control.text = "Custom" }
        mouseClick(control.actionButtonImage)
        tryCompare(control, "text", "Custom")
    }
}
