// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.BaseBanner"

    component: BaseBanner
    {
        width: 300
        text: "Message"
    }

    spiedSignals: ["closed", "closeClicked"]

    function showControl()
    {
        control.shown = true
        tryCompare(control, "opacity", 1)
    }

    function test_hiddenByDefault()
    {
        compare(control.shown, false)
        compare(control.closeable, true)
        compare(control.action, null)
        compare(control.opacity, 0)
        compare(control.visible, false)
    }

    function test_shown()
    {
        control.shown = true
        compare(control.state, "visible")
        tryCompare(control, "opacity", 1)
        compare(control.visible, true)
    }

    function test_closed()
    {
        showControl()
        control.shown = false
        compare(control.state, "hidden")
        tryCompare(signalSpy("closed"), "count", 1)
        tryCompare(control, "opacity", 0)
        compare(control.visible, false)
    }

    function test_closeClicked()
    {
        showControl()
        mouseClick(findById("closeButton"))
        tryCompare(signalSpy("closeClicked"), "count", 1)
        tryCompare(control, "shown", true)
    }

    function test_closeable()
    {
        const closeButton = findById("closeButton")
        showControl()
        compare(closeButton.visible, true)

        control.closeable = false
        compare(closeButton.visible, false)
    }

    function test_action()
    {
        const actionButton = findById("actionButton")
        showControl()
        compare(actionButton.visible, false)

        control.action = bannerAction
        compare(actionButton.visible, true)
        compare(actionButton.text, "Retry")

        actionTriggeredSpy.clear()
        mouseClick(actionButton)
        tryCompare(actionTriggeredSpy, "count", 1)
    }

    Action
    {
        id: bannerAction

        text: "Retry"
    }

    SignalSpy
    {
        id: actionTriggeredSpy

        target: bannerAction
        signalName: "triggered"
    }
}
