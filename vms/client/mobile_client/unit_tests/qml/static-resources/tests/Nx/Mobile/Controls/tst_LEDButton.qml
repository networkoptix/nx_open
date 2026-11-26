// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.LEDButton"

    component: LEDButton { text: "LED" }
    spiedSignals: ["clicked"]

    function test_indicatorColor()
    {
        verify(Qt.colorEqual(control.indicatorColor, ColorTheme.colors.dark12))
        verify(Qt.colorEqual(control.indicator.color, ColorTheme.colors.dark12))

        control.checked = true
        verify(Qt.colorEqual(control.indicatorColor, ColorTheme.colors.green_attention))
        verify(Qt.colorEqual(control.indicator.color, ColorTheme.colors.green_attention))
    }

    function test_checkedKeepsUncheckedBackground()
    {
        control.checked = true
        compare(control.state, "default_checked")
        verify(Qt.colorEqual(control.backgroundColor, ColorTheme.colors.dark8))
        compare(control.borderColor, "#00000000")
    }

    function test_textIndent()
    {
        control.spacing = 10
        compare(control.textIndent, control.indicator.width + 10)

        control.indicator = null
        compare(control.textIndent, 0)
    }

    function test_clicked()
    {
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
    }

    function test_pressedState()
    {
        mousePress(control)
        tryCompare(control, "state", "pressed_unchecked")
        mouseRelease(control)
        tryCompare(control, "state", "default_unchecked")
    }
}
