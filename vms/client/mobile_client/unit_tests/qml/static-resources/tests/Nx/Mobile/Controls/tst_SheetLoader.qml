// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.SheetLoader"

    component: SheetLoader
    {
        AdaptiveSheet {}
    }

    property QtObject createdSheet

    /** The loader is not a visual item, so it has no size to check. */
    function test_implicitSize()
    {
        verify(!(control instanceof Item))
    }

    function test_noSheetBeforeOpen()
    {
        compare(control.sheet, null)
        compare(control.opened, false)
    }

    function test_open()
    {
        control.open()
        verify(control.sheet instanceof AdaptiveSheet)
        tryCompare(control, "opened", true)
    }

    function test_sheetIsCreatedOnce()
    {
        control.open()
        tryCompare(control, "opened", true)

        const sheet = control.sheet
        sheet.close()
        tryCompare(control, "opened", false)

        control.open()
        compare(control.sheet, sheet)
        tryCompare(control, "opened", true)
    }

    function test_sheetIsDestroyedWithLoader()
    {
        control.open()
        tryCompare(control, "opened", true)
        createdSheet = control.sheet
        verify(createdSheet !== null)

        control.destroy()
        tryCompare(testCase, "createdSheet", null)
    }
}
