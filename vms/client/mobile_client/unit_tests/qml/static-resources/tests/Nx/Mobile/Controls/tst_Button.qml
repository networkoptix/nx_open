// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.Button"

    component: Button { text: "Button" }
    spiedSignals: ["clicked", "canceled"]

    function test_defaults()
    {
        compare(control.type, Button.Type.Interface)
        compare(control.showBusyIndicator, false)
    }

    function test_clicked()
    {
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
    }

    function test_canceled()
    {
        mousePress(control, control.width / 2, control.height / 2)
        mouseMove(control, -10, -10)
        mouseRelease(control, -10, -10)
        tryCompare(signalSpy("canceled"), "count", 1)
        tryCompare(signalSpy("clicked"), "count", 0)
    }

    function test_state()
    {
        compare(control.state, "default_unchecked")
        control.checked = true
        compare(control.state, "default_checked")
        control.enabled = false
        compare(control.state, "disabled")
    }

    function test_pressedState()
    {
        mousePress(control)
        tryCompare(control, "state", "pressed_unchecked")
        mouseRelease(control)
        tryCompare(control, "state", "default_unchecked")
    }

    function test_interfaceColors()
    {
        verify(Qt.colorEqual(control.backgroundColor, ColorTheme.colors.dark8))
        verify(Qt.colorEqual(control.foregroundColor, ColorTheme.colors.light10))
        compare(control.borderColor, "#00000000")

        control.checked = true
        verify(Qt.colorEqual(control.backgroundColor, ColorTheme.colors.dark10))
        verify(Qt.colorEqual(control.foregroundColor, ColorTheme.colors.light4))
        verify(Qt.colorEqual(control.borderColor, ColorTheme.colors.dark14))
    }

    function test_brandColors()
    {
        control.type = Button.Type.Brand
        verify(Qt.colorEqual(control.backgroundColor, ColorTheme.colors.brand_core))
        verify(Qt.colorEqual(control.foregroundColor, ColorTheme.colors.dark1))
        compare(control.borderColor, "#00000000")
    }

    function test_lightInterfaceColors()
    {
        control.type = Button.Type.LightInterface
        verify(Qt.colorEqual(control.backgroundColor, ColorTheme.colors.dark12))
        verify(Qt.colorEqual(control.foregroundColor, ColorTheme.colors.light10))

        control.checked = true
        verify(Qt.colorEqual(control.backgroundColor, ColorTheme.colors.dark14))
        verify(Qt.colorEqual(control.borderColor, ColorTheme.colors.dark18))
    }

    function test_busyIndicatorReplacesText()
    {
        const textItem = findById("textItem")
        const busyIndicator = findById("busyIndicator")
        compare(busyIndicator.visible, false)
        compare(textItem.visible, true)

        control.showBusyIndicator = true
        compare(busyIndicator.visible, true)
        compare(textItem.visible, false)
    }

    function test_icon()
    {
        const imageItem = findById("imageItem")
        compare(imageItem.visible, false)

        const textWidth = control.implicitWidth
        control.icon.source = "image://skin/20x20/Outline/close_medium.svg"
        compare(imageItem.visible, true)
        tryCompare(imageItem, "status", Image.Ready)
        compare(control.implicitWidth, textWidth + imageItem.implicitWidth + control.spacing)
    }
}
