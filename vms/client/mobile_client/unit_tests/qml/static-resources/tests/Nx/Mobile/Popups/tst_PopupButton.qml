// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Core
import Nx.Mobile.Popups
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Popups.PopupButton"

    component: PopupButton
    {
        width: 200
        height: 44
        text: "Button"
    }

    function test_defaults()
    {
        compare(control.accented, false)
    }

    function test_notAccented()
    {
        compare(control.flat, true)
        verify(Qt.colorEqual(control.textColor, ColorTheme.colors.light10))
        compare(control.background.color, "#00000000")
    }

    function test_accented()
    {
        control.accented = true
        compare(control.flat, false)
        verify(Qt.colorEqual(control.color, ColorTheme.colors.brand_core))
        verify(Qt.colorEqual(control.textColor, ColorTheme.colors.brand_contrast))
    }
}
