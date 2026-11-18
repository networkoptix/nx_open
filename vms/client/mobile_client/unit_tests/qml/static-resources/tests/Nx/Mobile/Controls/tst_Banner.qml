// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.Banner"

    component: Banner { shown: true }
    spiedSignals: ["closed", "closeClicked"]

    function test_shown()
    {
        compare(control.visible, true)
        compare(control.opacity, 1)

        control.shown = false
        tryCompare(control, "visible", false)
        tryCompare(control, "opacity", 0)
        compare(signalSpy("closed").count, 1)

        control.shown = true
        tryCompare(control, "visible", true)
        tryCompare(control, "opacity", 1)
        compare(signalSpy("closed").count, 1)
    }

    function test_closeable()
    {
        const closeButton = findById("closeButton")
        control.closeable = false
        compare(closeButton.visible, false)
        control.closeable = true
        compare(closeButton.visible, true)

        mouseClick(closeButton)
        compare(signalSpy("closeClicked").count, 1)
    }

    function test_action()
    {
        const actionButton = findById("actionButton")
        compare(actionButton.visible, false)
        control.action = createTemporaryObject(actionComponent)
        compare(actionButton.visible, true)

        mouseClick(actionButton)
        compare(control.action.triggerCount, 1)
    }

    function test_styles()
    {
        for (const type of Object.keys(control.styles))
        {
            control.type = type
            tryCompare(control.iconItem, "status", Image.Ready, /*timeout*/ 5000, `type "${type}"`)
        }
    }

    Component
    {
        id: actionComponent

        Action
        {
            property int triggerCount: 0

            text: "Action"
            onTriggered: triggerCount++
        }
    }
}
