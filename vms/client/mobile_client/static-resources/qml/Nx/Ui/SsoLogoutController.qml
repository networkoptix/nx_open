// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtWebView

// Terminates the SSO identity provider session after a cloud logout: drives a hidden web view
// through the provider logout URL from ssoLogoutUrlReady(), closing on isSsoLogoutComplete() or
// timeout.
Item
{
    id: controller

    function start(logoutUrl)
    {
        if (loader.active) //< A logout is already in progress; let it finish.
            return

        loader.active = true
        timeout.restart()
        loader.item.url = logoutUrl
    }

    function finish()
    {
        timeout.stop()
        loader.active = false
    }

    // Completion is decided by core (CloudStatusWatcher::isSsoLogoutComplete) so the desktop and
    // mobile clients share one rule. Guarded by loader.active (the in-progress state) so a stray
    // url/loading signal outside a logout is ignored.
    function checkComplete(url)
    {
        if (loader.active && appContext.cloudStatusWatcher.isSsoLogoutComplete(url))
            finish()
    }

    Connections
    {
        target: appContext.cloudStatusWatcher

        function onSsoLogoutUrlReady(logoutUrl) { controller.start(logoutUrl) }
    }

    // The web view is created on demand and destroyed when done, so no native web view is kept
    // alive while idle.
    Loader
    {
        id: loader

        active: false
        sourceComponent: webViewComponent
    }

    Component
    {
        id: webViewComponent

        WebView
        {
            // On Android the native WebView is wrapped as a foreign QWindow; its size/visibility
            // are pushed to the Android UI thread through Qt's window system, separately from the
            // view's own content rendering - so it can attach and paint full-screen before
            // visible:false/1x1 are applied (see qtwebview qandroidwebview.cpp). 1x1 only attempts
            // to bound that flash: size travels the same async path, so it may not apply in time
            // either.
            visible: false
            width: 1
            height: 1

            onUrlChanged: controller.checkComplete(url)
            onLoadingChanged: (loadRequest) =>
            {
                if (loadRequest.status === WebView.LoadSucceededStatus)
                    controller.checkComplete(url)
            }
        }
    }

    Timer
    {
        id: timeout

        interval: appContext.cloudStatusWatcher.ssoLogoutTimeoutMs()
        onTriggered: controller.finish()
    }
}
