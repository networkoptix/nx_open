// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Mobile.Controls

/**
 * Mock of the application main window, available in QML as `mainWindow`.
 *
 * Has only what the tested components use. A new member must have the name and the type it has
 * in main.qml, and it is restored in reset().
 *
 * Unlike WindowContextMock, no test compares the mock with main.qml, so a member which is renamed
 * or removed there must be changed here by hand.
 */
QtObject
{
    // The item which fills the window. ControlTestCase sets it to itself.
    property Item contentItem: null

    // A test sets it when the tested component shows banners.
    property WindowBanner banner: null

    function reset()
    {
        contentItem = null
        banner = null
    }
}
