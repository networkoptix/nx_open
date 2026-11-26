// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.SwipeControl"

    component: SwipeControl
    {
        width: 300
        height: 44

        contentItem: Rectangle
        {
            implicitWidth: 300
            implicitHeight: 44
        }

        leftItem: Button
        {
            implicitWidth: 60
            text: "L"
        }

        rightItem: Button
        {
            implicitWidth: 60
            text: "R"
        }
    }

    function test_defaults()
    {
        compare(control.shiftPosition, 0)
        compare(control.shift, 0)
        compare(control.shiftDirection, 0)
        compare(control.revealedItem, null)
    }

    function test_shiftIsRounded()
    {
        control.shiftPosition = 10.6
        compare(control.shift, 11)
        compare(control.shiftDirection, 1)

        control.shiftPosition = -10.6
        compare(control.shift, -11)
        compare(control.shiftDirection, -1)
    }

    function test_revealedItem()
    {
        control.shiftPosition = 49
        compare(control.revealedItem, null)

        control.shiftPosition = 50
        compare(control.revealedItem, control.leftItem)

        control.shiftPosition = -50
        compare(control.revealedItem, control.rightItem)
    }

    function test_revealDistance()
    {
        control.revealDistance = 20
        control.shiftPosition = 20
        compare(control.revealedItem, control.leftItem)
    }

    function test_sideItemsVisibility()
    {
        const leftControl = findById("leftControl")
        const rightControl = findById("rightControl")
        compare(leftControl.visible, false)
        compare(rightControl.visible, false)

        control.shiftPosition = 10
        compare(leftControl.visible, true)
        compare(rightControl.visible, false)

        control.shiftPosition = -10
        compare(leftControl.visible, false)
        compare(rightControl.visible, true)
    }

    function test_contentShiftIsBounded()
    {
        const content = findById("content")
        control.shiftPosition = 20
        compare(content.x, 20)

        control.shiftPosition = 200
        compare(content.x, 68)

        control.shiftPosition = -200
        compare(content.x, -68)
    }

    function test_spacing()
    {
        control.spacing = 20
        control.shiftPosition = 200
        compare(findById("content").x, 80)
    }

    function test_activateClicksRevealedItem()
    {
        leftClickedSpy.target = control.leftItem
        leftClickedSpy.clear()
        control.shiftPosition = 60
        control.activate()
        tryCompare(leftClickedSpy, "count", 1)
        tryCompare(control, "shiftPosition", 0)
    }

    function test_activateWithoutRevealedItem()
    {
        leftClickedSpy.target = control.leftItem
        leftClickedSpy.clear()
        control.shiftPosition = 20
        control.activate()
        compare(leftClickedSpy.count, 0)
        compare(control.shiftPosition, 20)
    }

    function test_activationAnimation()
    {
        activationStartedSpy.clear()
        control.activationAnimation = activationAnimation
        control.shiftPosition = 60
        control.activate()
        tryCompare(activationStartedSpy, "count", 1)
        compare(control.shiftPosition, 60)
    }

    function test_clickHidesRevealedItem()
    {
        control.shiftPosition = 68
        mouseClick(findById("content"))
        tryCompare(control, "shiftPosition", 0)
    }

    function test_dragRevealsLeftItem()
    {
        mouseDrag(control, 20, control.height / 2, 90, 0)
        tryCompare(control, "shiftPosition", 68)
        tryCompare(control, "revealedItem", control.leftItem)
    }

    function test_dragRevealsRightItem()
    {
        mouseDrag(control, 280, control.height / 2, -90, 0)
        tryCompare(control, "shiftPosition", -68)
        tryCompare(control, "revealedItem", control.rightItem)
    }

    function test_dragOverActivationDistanceActivates()
    {
        leftClickedSpy.target = control.leftItem
        leftClickedSpy.clear()
        mouseDrag(control, 20, control.height / 2, 150, 0)
        tryCompare(leftClickedSpy, "count", 1)
        tryCompare(control, "shiftPosition", 0)
    }

    function test_dragToHiddenItemIsIgnored()
    {
        control.leftItem.opacity = 0
        mouseDrag(control, 20, control.height / 2, 90, 0)
        tryCompare(control, "shiftPosition", 0)
    }

    SignalSpy
    {
        id: leftClickedSpy

        signalName: "clicked"
    }

    PauseAnimation
    {
        id: activationAnimation

        duration: 10
    }

    SignalSpy
    {
        id: activationStartedSpy

        target: activationAnimation
        signalName: "started"
    }
}
