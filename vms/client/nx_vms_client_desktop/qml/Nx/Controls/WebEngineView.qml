// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtWebEngine 1.10
import QtWebChannel 1.0

import nx.vms.client.desktop.utils 1.0

Item
{
    // Helper properties for providing additional JS APIs using Qt MetaObject system.
    property bool enableInjections: false

    Component
    {
        id: webChannelScript

        WebEngineScript
        {
            injectionPoint: WebEngineScript.DocumentCreation
            worldId: WebEngineScript.MainWorld
            name: "QWebChannel"
            sourceUrl: "qrc:///qtwebchannel/qwebchannel.js"
        }
    }

    Component
    {
        id: injectScript

        WebEngineScript
        {
            worldId: WebEngineScript.MainWorld
            injectionPoint: WebEngineScript.DocumentReady
            name: "NxObjectInjectorScript"
            sourceUrl: "qrc:///qml/Nx/Controls/web_engine_inject.js"
        }
    }

    // If set, then all connections are proxied via resourceId parent VMS Server.
    property string resourceId: ""

    property bool offTheRecord: false //< off-the-record means no info is saved on disk.

    // Original WebView which is opened inside scene or inside camera web settings.
    property var originalWebView: null
    // Window widget which contains current WebView. Remains null if this is originalWebView.
    property var containingWindow: null

    // WebViewController from C++, is used for opening dialogs, windows and menus.
    property var controller: null

    // Open links in desktop browser instead of following them.
    property bool redirectLinksToDesktop: false
    // Add context menu entries for web actions like Back, Forward, Stop and Reload.
    property bool menuNavigation: true
    // Add context menu entries for downloading images, pages etc.
    property bool menuSave: false

    property alias url: webView.url
    property alias userScripts: webView.userScripts
    property alias profile: webView.profile

    // Simplify access to WebEngineView subcomponent.
    readonly property var webView: webView

    signal loadingStatusChanged(int status)
    signal closeWindows() //< Close all child windows.

    Component.onDestruction: closeWindows()

    WebEngineView
    {
        id: webView
        objectName: "webView"

        anchors.fill: parent

        focus: true

        url: ""

        onCertificateError: function(error)
        {
            console.debug("Certificate error reported for", error.url)

            let accepted =
                controller && controller.verifyCertificate(error.certificateChain, error.url)
            console.debug("Native certificate validator returned", accepted)

            if (accepted)
                error.ignoreCertificateError()
            else
                error.rejectCertificate()
        }
        webChannel: enableInjections ? nxWebChannel : null
        userScripts: enableInjections
            ? [webChannelScript.createObject(webView), injectScript.createObject(webView)]
            : []

        profile: getProfile(offTheRecord, resourceId)

        function getProfile(offTheRecord, resourceId)
        {
            const hasWorkbench = typeof workbench !== "undefined" && workbench
            const profileName = hasWorkbench ? workbench.context.userId : ""
            return WebEngineProfileManager.getProfile(profileName, offTheRecord, resourceId)
        }

        onLoadingChanged: function(loadRequest)
        {
            if (loadRequest.status == WebEngineLoadRequest.LoadFailedStatus
                && loadRequest.errorDomain == WebEngineView.InternalErrorDomain)
            {
                // Content cannot be interpreted by Qt WebEngine, avoid continuous reloading.
                stop()
            }
            // Only report status for page url.
            if (loadRequest.url == webView.url)
                loadingStatusChanged(loadRequest.status)
        }

        onJavaScriptDialogRequested: showDialog("requestJavaScriptDialog", request)
        onAuthenticationDialogRequested: showDialog("requestAuthenticationDialog", request)
        onFileDialogRequested:  showDialog("requestFileDialog", request)
        onColorDialogRequested: showDialog("requestColorDialog", request)
        onSelectClientCertificate:
        {
            if (controller)
                controller.requestCertificateDialog(clientCertSelection)
            else
                clientCertSelection.selectNone()
        }

        onFeaturePermissionRequested: (securityOrigin, feature) =>
            grantFeaturePermission(securityOrigin, feature, false)

        function showDialog(methodName, request)
        {
            if (controller)
            {
                controller[methodName](request)
                return
            }

            // Controller is null when the page is about to be suspended, so prevent the page from
            // showing any dialogs.
            request.accepted = true
            request.dialogReject()
        }

        onNewViewRequested: function(request)
        {
            // If menu navigation is disabled, assume that content is intended to be view-only and
            // disable opening new windows.
            if (!menuNavigation)
                return

            // Do not create new windows when the page is about to be suspended.
            if (!controller)
                return

            let newWindow = controller.newWindow()

            const newGeometry = request.requestedGeometry
            const newWidth = newGeometry.width <= 0 ? width : newGeometry.width
            const newHeight = newGeometry.height <= 0 ? height : newGeometry.height

            newWindow.setWindowFrameGeometry(
                Qt.rect(newGeometry.x, newGeometry.y, newWidth, newHeight))

            newWindow.rootReady.connect(root =>
            {
                // Copy properties to the newly created WebEngineView.
                root.enableInjections = enableInjections
                root.userScripts = userScripts //< FIXME: what should we do with registerObjects?

                root.resourceId = resourceId
                root.profile = profile
                root.redirectLinksToDesktop = redirectLinksToDesktop
                root.menuNavigation = menuNavigation
                root.menuSave = menuSave

                root.offTheRecord = offTheRecord
                root.containingWindow = newWindow
                root.webView.windowCloseRequested.connect(root.containingWindow.close)
                root.originalWebView = originalWebView ? originalWebView : webView
                root.originalWebView.parent.closeWindows.connect(newWindow.close)
                request.openIn(root.webView)
            })

            newWindow.loadAndShow()
        }

        onTitleChanged:
        {
            // Change native window title to the web page title.
            if (containingWindow)
                containingWindow.windowTitle = title
        }

        onGeometryChangeRequested: function(geometry)
        {
            // Allow web page to position web dialog windows.
            if (containingWindow)
                containingWindow.setWindowFrameGeometry(geometry)
        }

        function getContextMenuActions(request)
        {
            // Setup context menu entries based on current settings.
            let actions = []

            if (menuNavigation)
            {
                const stopEnabled = webView.action(WebEngineView.Stop).enabled

                actions = [
                    WebEngineView.Back,
                    WebEngineView.Forward,
                    stopEnabled
                        ? WebEngineView.Stop
                        : WebEngineView.Reload
                ]
            }

            if (actions.length > 0)
                actions.push(WebEngineView.NoAction) //< Add separator.

            if (request && (request.editFlags & ContextMenuRequest.CanCut))
                actions.push(WebEngineView.Cut)

            actions.push(WebEngineView.Copy)

            if (request && (request.editFlags & ContextMenuRequest.CanPaste))
                actions.push(WebEngineView.Paste)

            // Check if linkUrl is an object. String convertion is important here.
            if (request && (request.linkUrl != ""))
                actions.push(WebEngineView.CopyLinkToClipboard)

            if (menuSave)
            {
                actions.push(WebEngineView.NoAction) //< Add separator.

                if (request)
                {
                    if (request.mediaType == ContextMenuRequest.MediaTypeImage)
                        actions.push(WebEngineView.DownloadImageToDisk)
                    else if (request.mediaType != ContextMenuRequest.MediaTypeNone)
                        actions.push(WebEngineView.DownloadMediaToDisk)
                }

                actions.push(WebEngineView.SavePage)
            }

            return actions.map(a => a === WebEngineView.NoAction ? null : webView.action(a))
        }

        onContextMenuRequested: function(request)
        {
            request.accepted = true

            if (!controller)
                return

            const actions = getContextMenuActions(request)

            controller.showMenu(request.x, request.y, actions)
        }

        onNavigationRequested: function(request)
        {
            if (!redirectLinksToDesktop)
                return

            if (request.navigationType == WebEngineNavigationRequest.LinkClickedNavigation)
            {
                request.action = WebEngineNavigationRequest.IgnoreRequest
                Qt.openUrlExternally(request.url)
            }
        }

        WebChannel
        {
            id: nxWebChannel
            objectName: "nxWebChannel"
        }
    }
}
