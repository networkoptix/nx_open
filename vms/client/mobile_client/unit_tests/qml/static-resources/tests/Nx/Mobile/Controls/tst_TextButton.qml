// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.TextButton"

    component: TextButton { width: 100; height: 32 }
    spiedSignals: ["clicked"]

    function test_text()
    {
        control.text = "Send"
        compare(findById("textItem").text, "Send")
    }

    function test_textColor()
    {
        control.textColor = "#ff0000"
        compare(control.textColor, "#ff0000")
        compare(findById("textItem").color, "#ff0000")
    }

    function test_font()
    {
        control.font.pixelSize = 20
        compare(findById("textItem").font.pixelSize, 20)
    }

    function test_implicitWidthFollowsText()
    {
        const textItem = findById("textItem")
        const emptyWidth = control.implicitWidth
        control.text = "Send"
        verify(control.implicitWidth > emptyWidth)
        const padding = control.leftPadding + control.rightPadding
        compare(control.implicitWidth, textItem.implicitWidth + padding)
    }

    function test_baselineOffset()
    {
        control.topPadding = 4
        compare(control.baselineOffset, 4 + findById("textItem").baselineOffset)
    }

    function test_clicked()
    {
        mouseClick(control)
        tryCompare(signalSpy("clicked"), "count", 1)
    }
}
