// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.WindowBanner"

    component: WindowBanner {}

    function createSource(properties)
    {
        const result = createTemporaryObject(sourceComponent, testCase, properties)
        verify(!!result, "Component exists")
        return result
    }

    /** The banner area has no implicit size: it fills the window overlay. */
    function test_implicitSize()
    {
        compare(control.width, testCase.width)
        compare(control.height, testCase.height)
    }

    function test_hiddenWithoutActiveSources()
    {
        createSource({text: "Message", type: Banner.Info})
        compare(control.activeItems.length, 0)
        compare(control.currentItem, null)
        compare(control.modalActive, false)
        compare(control.visible, false)
    }

    function test_activeSourceIsShown()
    {
        const source = createSource({text: "Message", type: Banner.Warning})
        source.trigger()
        compare(control.activeItems.length, 1)
        compare(control.currentItem, source)
        compare(control.visible, true)

        const banner = findById("banner")
        compare(banner.shown, true)
        compare(banner.text, "Message")
        compare(banner.type, Banner.Warning)
        compare(banner.closeable, false)
        tryCompare(banner, "opacity", 1)
    }

    function test_closeCurrent()
    {
        const source = createSource({text: "Message", type: Banner.Info})
        source.trigger()
        compare(control.currentItem, source)

        control.closeCurrent()
        compare(source.active, false)
        tryCompare(control, "currentItem", null)
        compare(control.visible, false)
    }

    function test_closeCurrentKeepsNotCloseable()
    {
        const source = createSource({text: "Message", type: Banner.Error})
        source.trigger()
        compare(findById("timer").running, false)

        control.closeCurrent()
        compare(source.active, true)
        compare(control.currentItem, source)
    }

    function test_nextSourceIsShownAfterCurrent()
    {
        const first = createSource({text: "First", type: Banner.Error})
        const second = createSource({text: "Second", type: Banner.Error})
        first.trigger()
        second.trigger()
        compare(control.activeItems.length, 2)
        compare(control.currentItem, first)

        first.reset()
        tryCompare(control, "currentItem", second)
        compare(findById("banner").text, "Second")
    }

    function test_sourceIsClosedAfterDuration()
    {
        const source = createSource({text: "Message", type: Banner.Info, durationMs: 50})
        source.trigger()
        compare(findById("timer").running, true)
        tryCompare(source, "active", false)
        tryCompare(control, "currentItem", null)
    }

    function test_modalActive()
    {
        const source = createSource({text: "Message", type: Banner.Error, modal: true})
        source.trigger()
        compare(control.modalActive, true)

        source.reset()
        tryCompare(control, "modalActive", false)
    }

    function test_closeButton()
    {
        const source = createSource({text: "Message", type: Banner.Info})
        source.trigger()

        const banner = findById("banner")
        tryCompare(banner, "opacity", 1)
        mouseClick(findById("closeButton", banner))
        tryCompare(source, "active", false)
        tryCompare(control, "currentItem", null)
    }

    function test_show()
    {
        mainWindow.banner = control
        control.show("Message", Banner.Success)
        compare(control.activeItems.length, 1)

        const banner = findById("banner")
        tryCompare(banner, "text", "Message")
        compare(banner.type, Banner.Success)
        compare(control.visible, true)
    }

    Component
    {
        id: sourceComponent

        BannerSource { target: testCase.control }
    }
}
