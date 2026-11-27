// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Popups
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Popups.LogoutConfirmationPopup"

    component: LogoutConfirmationPopup {}

    function openControl()
    {
        control.open()
        tryCompare(control, "opened", true)

        // The content is laid out on polish, after open() has returned.
        verify(waitForPolish(Window.window))
    }

    function watchStopSession()
    {
        stopSessionSpy.target = windowContext.sessionManager
        stopSessionSpy.clear()
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

    function test_iconLoaded()
    {
        openControl()

        const icon = findById("icon")
        tryCompare(icon, "status", Image.Ready)
        compare(icon.visible, true)
    }

    function test_localSessionMessage()
    {
        windowContext.mainSystemContext.userWatcher.userFullName = "Local User"
        windowContext.sessionManager.systemName = "Site"
        cloudUserProfileWatcher.fullName = "Cloud User"
        compare(control.messages.length, 1)
        compare(control.messages[0], "Disconnect Local User from Site?")
    }

    function test_cloudSessionMessage()
    {
        windowContext.mainSystemContext.userWatcher.userFullName = "Local User"
        windowContext.sessionManager.systemName = "Site"
        windowContext.sessionManager.isCloudSession = true
        cloudUserProfileWatcher.fullName = "Cloud User"
        compare(control.messages.length, 1)
        compare(control.messages[0], "Disconnect Cloud User from Site?")
    }

    function test_cancelCloses()
    {
        openControl()
        watchStopSession()
        mouseClick(findById("cancelButton"))
        tryCompare(control, "visible", false)
        compare(stopSessionSpy.count, 0)
    }

    function test_disconnectStopsSession()
    {
        openControl()
        watchStopSession()
        mouseClick(findById("disconnectButton"))
        tryCompare(stopSessionSpy, "count", 1)
        tryCompare(control, "visible", false)
    }

    SignalSpy
    {
        id: stopSessionSpy

        signalName: "stopSessionByUserCalled"
    }
}
