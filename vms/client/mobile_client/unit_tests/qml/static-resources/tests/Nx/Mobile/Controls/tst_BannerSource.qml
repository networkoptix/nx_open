// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.BannerSource"

    component: BannerSource { target: bannerTarget }

    /** The source is an invisible helper without a size. */
    function test_implicitSize()
    {
        compare(control.visible, false)
        compare(control.width, 0)
        compare(control.height, 0)
    }

    function test_defaults()
    {
        compare(control.type, Banner.Info)
        compare(control.text, "")
        compare(control.action, null)
        compare(control.active, false)
        compare(control.modal, false)
    }

    function test_defaultTargetIsWindowBanner()
    {
        const windowBanner = createTemporaryObject(windowBannerComponent, testCase)
        verify(!!windowBanner, "Component exists")
        mainWindow.banner = windowBanner

        const source = createTemporaryObject(defaultSourceComponent, testCase)
        verify(!!source, "Component exists")
        compare(source.target, windowBanner)
        compare(source.parent, windowBanner)
    }

    function test_closeable_data()
    {
        return [
            {tag: "info", type: Banner.Info, closeable: true},
            {tag: "success", type: Banner.Success, closeable: true},
            {tag: "warning", type: Banner.Warning, closeable: false},
            {tag: "error", type: Banner.Error, closeable: false}
        ]
    }

    function test_closeable(data)
    {
        control.type = data.type
        compare(control.closeable, data.closeable)
    }

    function test_durationMs_data()
    {
        return [
            {tag: "info", type: Banner.Info, durationMs: 4000},
            {tag: "success", type: Banner.Success, durationMs: 4000},
            {tag: "warning", type: Banner.Warning, durationMs: null},
            {tag: "error", type: Banner.Error, durationMs: null}
        ]
    }

    function test_durationMs(data)
    {
        control.type = data.type
        compare(control.durationMs, data.durationMs)
    }

    function test_noDurationWithAction()
    {
        control.type = Banner.Info
        control.action = bannerAction
        compare(control.closeable, true)
        compare(control.durationMs, null)
    }

    function test_noDurationWhenNotCloseable()
    {
        control.type = Banner.Info
        control.closeable = false
        compare(control.durationMs, null)
    }

    function test_triggerAndReset()
    {
        control.trigger()
        compare(control.active, true)

        control.reset()
        compare(control.active, false)
    }

    Item
    {
        id: bannerTarget
    }

    Action
    {
        id: bannerAction

        text: "Action"
    }

    Component
    {
        id: windowBannerComponent

        WindowBanner {}
    }

    Component
    {
        id: defaultSourceComponent

        BannerSource {}
    }
}
