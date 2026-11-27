// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtTest

import Nx.Mobile.Popups
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Popups.PopupBase"

    component: PopupBase
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

    function messageItem(index)
    {
        const result = findById("messagesRepeater").itemAt(index)
        verify(!!result, "Object exists")
        return result
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
        compare(control.autoClose, false)
        compare(control.modal, true)
        compare(control.messagesTextAlignment, Text.AlignLeft)

        openControl()
        compare(control.withCloseButton, true)
    }

    function test_closePolicy()
    {
        compare(control.closePolicy, Popup.NoAutoClose)
        control.autoClose = true
        compare(control.closePolicy,
            Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside)
    }

    function test_closeButtonCloses()
    {
        openControl()
        mouseClick(findById("closeButton"))
        tryCompare(control, "visible", false)
    }

    function test_widthIsLimitedByParent()
    {
        mainWindow.contentItem = narrowParent
        const popup = createTemporaryObject(component, testCase)
        verify(!!popup, "Component exists")
        compare(popup.width, narrowParent.width - popup.gap * 2)
    }

    function test_heightIsLimitedByParent()
    {
        control.messages = Array(20).fill("Message")
        openControl()
        tryCompare(control, "height", testCase.height - control.gap * 2)
        verify(control.contentHeight > control.height)
    }

    function test_shadowsFollowScrollPosition()
    {
        control.messages = Array(20).fill("Message")
        openControl()

        const topShadow = findById("topShadow")
        const bottomShadow = findById("bottomShadow")
        tryCompare(bottomShadow, "opacity", 1)
        compare(topShadow.visible, false)

        const scrollBar = findById("scrollBar")
        scrollBar.position = 1 - scrollBar.size
        tryCompare(topShadow, "opacity", 1)
        fuzzyCompare(bottomShadow.opacity, 0, 0.001)
    }

    function test_noShadowsWithoutOverflow()
    {
        openControl()
        compare(findById("scrollBar").size, 1)
        compare(findById("topShadow").visible, false)
        compare(findById("bottomShadow").visible, false)
    }

    function test_layoutWithoutIcon()
    {
        openControl()
        compare(findById("icon").visible, false)
        compare(findById("separator").visible, true)

        const title = findById("title")
        compare(title.horizontalAlignment, Text.AlignLeft)
        compare(title.Layout.bottomMargin, 24)
        compare(title.Layout.rightMargin, 24 + findById("closeButton").width + 12)
    }

    /** No place is kept to the right of the title when there is no close button. */
    function test_layoutWithoutCloseButton()
    {
        control.withCloseButton = false
        openControl()
        compare(findById("closeButton").visible, false)

        const title = findById("title")
        compare(title.Layout.rightMargin, title.Layout.leftMargin)
    }

    function test_layoutWithIcon()
    {
        control.icon = "image://skin/48x48/Solid/warning.svg?primary=yellow"
        openControl()

        const icon = findById("icon")
        tryCompare(icon, "status", Image.Ready)
        compare(icon.visible, true)
        compare(findById("separator").visible, false)

        const title = findById("title")
        compare(title.horizontalAlignment, Text.AlignHCenter)
        compare(title.Layout.bottomMargin, 12)
        compare(title.Layout.rightMargin, 24)
    }

    function test_titleAndMessagesAreShown()
    {
        control.messages = ["First", "Second"]
        openControl()
        compare(findById("title").text, "Title")
        compare(findById("messagesRepeater").count, 2)
        compare(messageItem(0).text, "First")
        compare(messageItem(1).text, "Second")
    }

    function test_messagesTextAlignment()
    {
        openControl()
        compare(messageItem(0).horizontalAlignment, Text.AlignLeft)
        control.messagesTextAlignment = Text.AlignHCenter
        compare(messageItem(0).horizontalAlignment, Text.AlignHCenter)
    }

    function test_emptyMessageIsHidden()
    {
        control.messages = ["First", "", "Last"]
        openControl()
        compare(messageItem(0).visible, true)
        compare(messageItem(1).visible, false)
        compare(messageItem(2).visible, true)
    }

    Item
    {
        id: narrowParent

        width: 300
        height: 400
    }
}
