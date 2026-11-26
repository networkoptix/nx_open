// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.ToolBarButton"

    component: ToolBarButton
    {
        icon.source: "image://skin/24x24/Outline/close.svg?primary=light10"
    }

    spiedSignals: ["pressed", "released", "clicked", "canceled"]

    function test_clicked()
    {
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
        tryCompare(signalSpy("pressed"), "count", 1)
        tryCompare(signalSpy("released"), "count", 1)
        tryCompare(signalSpy("canceled"), "count", 0)
    }

    function test_canceled()
    {
        mousePress(control, control.width / 2, control.height / 2)
        mouseMove(control, -10, -10)
        mouseRelease(control, -10, -10)
        tryCompare(signalSpy("canceled"), "count", 1)
        tryCompare(signalSpy("clicked"), "count", 0)
    }

    function test_down()
    {
        mousePress(control)
        tryCompare(control, "down", true)
        mouseRelease(control)
        tryCompare(control, "down", false)
    }

    function test_clickTogglesCheckable()
    {
        control.checkable = true
        mouseClick(control)
        tryCompare(control, "checked", true)
        mouseClick(control)
        tryCompare(control, "checked", false)
    }

    function test_clickDoesNotCheckNotCheckable()
    {
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
        tryCompare(control, "checked", false)
    }

    function test_toggle()
    {
        control.toggle()
        compare(control.checked, false)

        control.checkable = true
        control.toggle()
        compare(control.checked, true)
    }

    function test_notCheckableIsUnchecked()
    {
        control.checkable = true
        control.checked = true
        control.checkable = false
        compare(control.checked, false)
    }
}
