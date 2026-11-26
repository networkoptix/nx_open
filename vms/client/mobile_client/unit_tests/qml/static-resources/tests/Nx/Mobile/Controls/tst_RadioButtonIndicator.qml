// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.RadioButtonIndicator"

    component: RadioButtonIndicator {}

    function test_unchecked()
    {
        const circle = findById("circle")
        compare(control.checked, false)
        verify(Qt.colorEqual(circle.color, ColorTheme.colors.dark11))
        verify(Qt.colorEqual(circle.border.color, ColorTheme.colors.dark13))
        compare(findById("dot").visible, false)
    }

    function test_checked()
    {
        const circle = findById("circle")
        control.checked = true
        verify(Qt.colorEqual(circle.color, ColorTheme.colors.attention.green))
        compare(circle.border.color, "#00000000")
        compare(findById("dot").visible, true)
    }

    function test_uncheckedAgain()
    {
        const circle = findById("circle")
        control.checked = true
        control.checked = false
        verify(Qt.colorEqual(circle.color, ColorTheme.colors.dark11))
        compare(findById("dot").visible, false)
    }

    function test_disabledOpacity()
    {
        compare(control.opacity, 1)
        control.enabled = false
        compare(control.opacity, 0.3)

        control.enabled = true
        compare(control.opacity, 1)
    }

    function test_layerOnlyForDisabledChecked()
    {
        const circle = findById("circle")
        compare(circle.layer.enabled, false)

        control.checked = true
        compare(circle.layer.enabled, false)

        control.enabled = false
        compare(circle.layer.enabled, true)

        control.checked = false
        compare(circle.layer.enabled, false)
    }
}
