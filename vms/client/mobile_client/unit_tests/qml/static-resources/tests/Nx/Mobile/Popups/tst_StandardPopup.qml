// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtTest

import Nx.Mobile.Popups
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Popups.StandardPopup"

    component: StandardPopup
    {
        title: "Title"
        messages: ["Message"]
    }

    function openControl()
    {
        control.open()
        tryCompare(control, "opened", true)

        // The content is laid out on polish, after open() has returned.
        verify(waitForPolish(Window.window))
    }

    /** The popup has no implicit width: its size is set by the parent and by the content. */
    function test_implicitSize()
    {
        openControl()
        compare(control.width, 328)
        verify(control.height > 0)
        compare(control.height, control.contentHeight)
        verify(control.height <= testCase.height)
    }

    function test_defaults()
    {
        compare(control.accentedOkButton, false)

        openControl()
        compare(control.withCloseButton, false)
    }

    /** There is no close button, so no place is kept for it to the right of the title. */
    function test_titleMargins()
    {
        openControl()

        const title = findById("title")
        compare(title.Layout.rightMargin, title.Layout.leftMargin)
    }

    function test_okButtonCloses()
    {
        openControl()
        mouseClick(findById("okButton"))
        tryCompare(control, "visible", false)
    }
}
