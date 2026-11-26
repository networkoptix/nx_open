// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.ButtonBar"

    component: ButtonBar
    {
        Button { text: "One" }
        Button { text: "Two" }
        Button { text: "Three" }
    }

    function spacerAt(index)
    {
        const result = findById("spacerRepeater").itemAt(index)
        verify(!!result, "Object exists")
        return result
    }

    function test_buttonsAreInRow()
    {
        const first = control.itemAt(0)
        verify(!!first, "Object exists")
        const second = control.itemAt(1)
        verify(!!second, "Object exists")
        const third = control.itemAt(2)
        verify(!!third, "Object exists")

        compare(second.y, first.y)
        compare(third.y, first.y)
        compare(second.x, first.x + first.width + control.spacing)
        compare(third.x, second.x + second.width + control.spacing)
    }

    function test_implicitSizeFitsButtons()
    {
        const buttonRow = findById("buttonRow")
        const third = control.itemAt(2)
        verify(!!third, "Object exists")

        compare(buttonRow.width, third.x + third.width)
        compare(control.implicitWidth, buttonRow.width)
        compare(control.implicitHeight, buttonRow.height)
    }

    function test_buttonClicked()
    {
        const button = control.itemAt(1)
        verify(!!button, "Object exists")
        buttonClickedSpy.target = button
        buttonClickedSpy.clear()
        mouseClick(button)
        tryCompare(buttonClickedSpy, "count", 1)
    }

    function test_spacersAreBetweenButtons()
    {
        compare(findById("spacerRepeater").count, 3)
        tryCompare(spacerAt(0), "relevant", true)
        tryCompare(spacerAt(1), "relevant", true)
        tryCompare(spacerAt(2), "relevant", false)
        compare(spacerAt(0).visible, true)
        compare(spacerAt(1).visible, true)
        compare(spacerAt(2).visible, false)
    }

    function test_spacerGeometry()
    {
        const first = control.itemAt(0)
        verify(!!first, "Object exists")
        const second = control.itemAt(1)
        verify(!!second, "Object exists")

        tryCompare(spacerAt(0), "x", first.x + first.width)
        tryCompare(spacerAt(1), "x", second.x + second.width)
        compare(spacerAt(0).width, control.spacing)
        compare(spacerAt(0).height, control.contentItem.height)
    }

    function test_hiddenButtonHasNoSpacer()
    {
        const third = control.itemAt(2)
        verify(!!third, "Object exists")

        third.visible = false
        tryCompare(spacerAt(1), "relevant", false)
        tryCompare(spacerAt(2), "relevant", false)
        tryCompare(spacerAt(0), "relevant", true)
    }

    function test_spacerDelegate()
    {
        control.spacerDelegate = spacerComponent

        const spacer = findChild(spacerAt(0), "spacer")
        verify(!!spacer, "Object exists")
        compare(spacer.width, 3)
        compare(findChild(spacerAt(2), "spacer"), null)
    }

    function test_noSpacerDelegate()
    {
        control.spacerDelegate = null
        compare(findById("spacerRepeater").count, 0)
    }

    SignalSpy
    {
        id: buttonClickedSpy

        signalName: "clicked"
    }

    Component
    {
        id: spacerComponent

        Rectangle
        {
            objectName: "spacer"

            width: 3
            height: 20
        }
    }
}
