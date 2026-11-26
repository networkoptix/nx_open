// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.ToolTip"

    component: ToolTip
    {
        id: toolTip

        target: targetItem
        text: "Hint"

        contentItem: Text { text: toolTip.text }
    }

    function openControl()
    {
        control.open()
        tryCompare(control, "opened", true)
    }

    function test_targetSize()
    {
        compare(control.targetSize.width, 100)
        compare(control.targetSize.height, 40)
    }

    function test_noTarget()
    {
        control.target = null
        compare(control.targetSize.width, 0)
        compare(control.targetSize.height, 0)
    }

    function test_positionIsUnderTargetCenter()
    {
        openControl()
        compare(control.x, 50 - control.width / 2)
        compare(control.y, 45)
    }

    function test_targetSpacing()
    {
        control.targetSpacing = 4
        openControl()
        compare(control.y, 49)
    }

    function test_arrowSize()
    {
        control.arrowSize = Qt.size(20, 10)
        openControl()
        compare(control.y, 50)
    }

    function test_targetRect()
    {
        openControl()
        compare(control.targetRect.x, -control.x)
        compare(control.targetRect.y, -45)
        compare(control.targetRect.width, 100)
        compare(control.targetRect.height, 40)
    }

    function test_bubblePointsAtTargetCenter()
    {
        openControl()
        compare(control.background.target.x, control.width / 2)
        compare(control.background.target.y, -25)
    }

    function test_openClose()
    {
        openControl()
        tryCompare(control, "opacity", 1)

        control.close()
        tryCompare(control, "visible", false)
    }

    Item
    {
        id: targetItem

        x: 150
        y: 100
        width: 100
        height: 40
    }
}
