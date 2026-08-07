// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtWebView 1.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Ui 1.0

import nx.vms.client.core 1.0

Page
{
    id: screen

    objectName: "loginToCloudScreen"

    property string token
    property string user

    property bool forced: false

    signal gotResult(value: string)

    title: qsTr("Log In")

    onLeftButtonClicked: Workflow.popCurrentScreen()

    WebView
    {
        id: webView

        onUrlChanged: d.handleUrlChanged(url)

        onLoadingChanged: (loadRequest) =>
        {
            switch (loadRequest.status)
            {
                case WebView.LoadStartedStatus:
                    d.paintWebViewBackground()
                    break
                case WebView.LoadFailedStatus:
                    d.cancelLogInProcess()
                    break
                case WebView.LoadSucceededStatus:
                    d.paintWebViewBackground()

                    // Workaround for the size bugs in WebView.
                    webView.width = Qt.binding(()=>screen.width)
                    webView.height = Qt.binding(()=>
                    {
                        const minimalOffset = Qt.platform.os === "android"
                            ? screen.header.height + windowContext.ui.measurements.androidKeyboardHeight
                            : 0
                        return screen.height - minimalOffset
                    })
                    break
                default:
                    break
            }
        }
    }

    NxObject
    {
        id: d

        property OauthClient oauthClient

        // The native web view is opaque and white and is layered above the Qt scene, so nothing
        // drawn in QML can hide it while a page loads. Called repeatedly because the view is
        // attached to the platform hierarchy asynchronously.
        function paintWebViewBackground()
        {
            windowContext.ui.windowHelpers.setWebViewBackgroundColor(screen.backgroundColor)
        }

        function cancelLogInProcess()
        {
            webView.visible = false
            const title = qsTr("Cannot connect to %1",
                "%1 is the short cloud name (like 'Cloud')")
                    .arg(appContext.appInfo.cloudName())
            const message = appContext.pushManager.checkConnectionErrorText()
            // The screen owns the dialog: errors may arrive while the screen is being destroyed.
            const warning = Workflow.openStandardDialog(title, message,
                ["OK"], /*disableAutoClose*/ true, screen)
            warning.buttonClicked.connect(
                function(buttonId)
                {
                    Workflow.popCurrentScreen()
                })
        }

        function handleUrlChanged(url)
        {
            const u = NxGlobals.url(url)
            if (u.path() !== '/redirect-oauth')
                return

            const code = u.queryItem('code')
            if (!code)
                return

            redirectUrlWorkaround.stop()

            screen.gotResult("success")
            d.oauthClient.setCode(code)
        }

        // Actual page url is not updated via onUrlChanged on some platforms, Android among them,
        // so the redirect carrying the authorization code has to be polled for. The url is read
        // off the web view instead of being fetched with runJavaScript(): since Qt 6.11 that
        // callback is invoked from the platform thread rather than through a queued signal, which
        // enters the QML engine from a thread that does not own it and crashes the application.
        Timer
        {
            id: redirectUrlWorkaround

            running: true
            repeat: true
            interval: 1000
            onTriggered: d.handleUrlChanged(webView.url)
        }
    }

    Component.onCompleted:
    {
        const helper = appContext.credentialsHelper.createOauthClient(token, user, forced)
        const closePage = ()=>Workflow.popCurrentScreen()
        helper.authDataReady.connect(closePage)
        helper.cancelled.connect(closePage)
        helper.setLocale(locale.name)

        d.oauthClient = helper
        webView.url = d.oauthClient.url
        d.paintWebViewBackground()
    }
}
