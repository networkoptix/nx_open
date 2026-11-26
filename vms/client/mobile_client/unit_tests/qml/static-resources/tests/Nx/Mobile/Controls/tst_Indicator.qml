// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.Indicator"

    component: Indicator {}

    /** The indicator sets its size itself and has no implicit size. */
    function test_implicitSize()
    {
        compare(control.implicitWidth, 0)
        compare(control.implicitHeight, 0)
    }

    function test_dotWithoutText()
    {
        compare(control.text, "")
        compare(control.width, 6)
        compare(control.height, 6)
        compare(control.radius, 3)
        compare(findById("textItem").visible, false)
    }

    function test_badgeWithText()
    {
        const textItem = findById("textItem")
        control.text = "5"
        compare(textItem.text, "5")
        compare(textItem.visible, true)
        compare(control.width, 12)
        compare(control.height, 12)
        compare(control.radius, 6)
    }

    function test_badgeGrowsWithText()
    {
        const textItem = findById("textItem")
        control.text = "1234567"
        verify(textItem.width > 12)
        compare(control.width, textItem.width)
        compare(control.height, 12)
    }

    function test_textIsCleared()
    {
        control.text = "5"
        control.text = ""
        compare(control.width, 6)
        compare(control.height, 6)
        compare(findById("textItem").visible, false)
    }
}
