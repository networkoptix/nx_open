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

    signal gotResult(value: string)

    onLeftButtonClicked: Workflow.popCurrentScreen()

    WebView
    {
        id: webView

        url: d.oauthClient.url

        onUrlChanged: d.handleUrlChanged(url)

        onLoadingChanged: (loadRequest) =>
        {
            switch (loadRequest.status)
            {
                case WebView.LoadFailedStatus:
                    d.cancelLogInProcess()
                    break
                case WebView.LoadSucceededStatus:
                    // Workaround for the size bugs in WebView.
                    webView.width = Qt.binding(()=>screen.width)
                    webView.height = Qt.binding(()=>
                    {
                        const minimalOffset = Qt.platform.os === "android"
                            ? screen.header.height + androidKeyboardHeight
                            : 0
                        return screen.height - minimalOffset
                    })
                    webView.forceActiveFocus()
                    break
                default:
                    break
            }
        }
    }

    NxObject
    {
        id: d

        property OauthClient oauthClient:
        {
            const helper = createOauthClient(token, user)
            const closePage = ()=>Workflow.popCurrentScreen()
            helper.authDataReady.connect(closePage)
            helper.cancelled.connect(closePage)
            helper.setLocale(locale.name)
            return helper
        }

        function cancelLogInProcess()
        {
            webView.visible = false
            const title = qsTr("Cannot connect to %1",
                "%1 is the short cloud name (like 'Cloud')").arg(applicationInfo.cloudName())
            const message = pushManager.checkConnectionErrorText()
            const warning = Workflow.openStandardDialog(title, message,
                /*buttonsModel*/ null, /*disableAutoClose*/ true)
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

            iosRedirectUrlWorkaround.stop()

            screen.gotResult("success")
            d.oauthClient.setCode(code)
        }

        Timer
        {
            id: iosRedirectUrlWorkaround

            running: Qt.platform.os === "ios"
            repeat: true
            interval: 1000
            onTriggered: d.handleUrlChanged(getWebkitUrl())
        }
    }
}
