// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtWebEngine
import QtWebChannel

import nx.vms.client.desktop.utils

Item
{
    // Helper properties for providing additional JS APIs using Qt MetaObject system.
    property bool enableInjections: false
    onEnableInjectionsChanged: loadUserScripts()

    function loadUserScripts()
    {
        if (!enableInjections)
        {
            webView.userScripts.clear()
            return
        }

        webView.userScripts.collection = [
            {
                worldId: WebEngineScript.MainWorld,
                injectionPoint: WebEngineScript.DocumentReady,
                name: "NxObjectInjectorScript",
                sourceUrl: "qrc:///qml/Nx/Controls/web_engine_inject.js"
            },
            {
                injectionPoint: WebEngineScript.DocumentCreation,
                worldId: WebEngineScript.MainWorld,
                name: "QWebChannel",
                sourceUrl: "qrc:///qtwebchannel/qwebchannel.js"
            }
        ]
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

    // Enable web page to have its own context menu.
    property bool enableCustomMenu: false

    // Sets whether to load a web page icon.
    property bool loadIcon: false

    property alias url: webView.url
    property alias userScripts: webView.userScripts
    property alias profile: webView.profile

    // Simplify access to WebEngineView subcomponent.
    readonly property var webView: webView

    signal loadingStatusChanged(int status)
    signal closeWindows() //< Close all child windows.

    Component.onCompleted: loadUserScripts()
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
                controller && controller.verifyCertificate(error)
            console.debug("Native certificate validator returned", accepted)

            if (accepted)
                error.acceptCertificate()
            else
                error.rejectCertificate()
        }
        webChannel: enableInjections ? nxWebChannel : null

        profile: getProfile(offTheRecord, resourceId)

        function getProfile(offTheRecord, resourceId)
        {
            const hasWorkbench = typeof workbench !== "undefined" && workbench
            const profileName = hasWorkbench ? workbench.context.userId : ""
            return WebEngineProfileManager.getProfile(profileName, offTheRecord, resourceId)
        }

        onLoadingChanged: function(loadRequest)
        {
            if (loadRequest.status == WebEngineDownloadRequest.LoadFailedStatus
                && loadRequest.errorDomain == WebEngineView.InternalErrorDomain)
            {
                // Content cannot be interpreted by Qt WebEngine, avoid continuous reloading.
                stop()
            }
            // Only report status for page url.
            if (loadRequest.url == webView.url)
                loadingStatusChanged(loadRequest.status)
        }

        onJavaScriptDialogRequested: request => showDialog("requestJavaScriptDialog", request)
        onAuthenticationDialogRequested:
            request => showDialog("requestAuthenticationDialog", request)
        onFileDialogRequested: request => showDialog("requestFileDialog", request)
        onColorDialogRequested: request => showDialog("requestColorDialog", request)
        onSelectClientCertificate:
        {
            if (controller)
                controller.requestCertificateDialog(clientCertSelection)
            else
                clientCertSelection.selectNone()
        }

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

        onNewWindowRequested: function(request)
        {
            // If menu navigation is disabled, assume that content is intended to be view-only and
            // disable opening new windows.
            if (!menuNavigation)
                return

            // Do not create new windows when the page is about to be suspended.
            if (!controller)
                return

            // Open tab in the same item. New window can be opened using window.open() js method.
            if (request.destination === WebEngineNewWindowRequest.InNewTab
                || request.destination === WebEngineNewWindowRequest.InNewBackgroundTab)
            {
                webView.url = request.requestedUrl
                return
            }

            let newWindow = controller.newWindow()

            const kDefaultSize = Qt.size(800, 740)
            const newGeometry = request.requestedGeometry
            const newWidth = newGeometry.width <= 0 ? kDefaultSize.width : newGeometry.width
            const newHeight = newGeometry.height <= 0 ? kDefaultSize.height : newGeometry.height

            if (newGeometry.x <= 0 && newGeometry.y <= 0) //< No position is specified.
            {
                newWindow.setWindowSize(newWidth, newHeight)
            }
            else
            {
                newWindow.setWindowFrameGeometry(
                    Qt.rect(newGeometry.x, newGeometry.y, newWidth, newHeight))
            }

            newWindow.rootReady.connect(root =>
            {
                // Copy properties to the newly created WebEngineView.
                root.enableInjections = enableInjections
                root.userScripts.collection = userScripts.collection //< FIXME: what should we do with registerObjects?

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

        Component
        {
            id: devToolsComponent

            Window
            {
                readonly property var webView: devToolsWebView

                width: 700
                height: 500

                onClosing: devToolsWebView.inspectedView = null

                WebEngineView
                {
                    id: devToolsWebView
                    anchors.fill: parent
                }
            }
        }

        function showDevTools()
        {
            if (devToolsView)
            {
                // Just raise already opened window.
                devToolsView.Window.window.raise()
                return
            }

            const devTools = devToolsComponent.createObject()
            devToolsView = devTools.webView
            parent.closeWindows.connect(devTools.close)

            devTools.show()
            devTools.raise()
        }

        onContextMenuRequested: function(request)
        {
            if (enableCustomMenu)
            {
                request.accepted = false
                return
            }

            request.accepted = true

            if (!controller)
                return

            const actions = getContextMenuActions(request)

            controller.showMenu(request.position.x, request.position.y, actions)
        }

        onNavigationRequested: function(request)
        {
            // When webView is empty there should be no redirects.
            if (webView.url == "" && request.isMainFrame)
                return

            if (!redirectLinksToDesktop)
                return

            if (request.navigationType == WebEngineNavigationRequest.LinkClickedNavigation)
            {
                request.reject()
                Qt.openUrlExternally(request.url)
            }
        }

        WebChannel
        {
            id: nxWebChannel
            objectName: "nxWebChannel"
        }
    }

    Image
    {
        id: icon

        visible: false
        sourceSize: Qt.size(40, 40)

        // Load the icon directly (favicon image provider can not load some formats).
        source: loadIcon ? String(webView.icon).replace("image://favicon/", "") : ""

        onStatusChanged:
        {
            if (status !== Image.Ready || !controller)
                return;

            const resourceUrl = controller.resourceUrl()
            const iconCache = controller.iconCache()
            const currentHost = (new URL(webView.url)).host
            const webPageHost = resourceUrl ? (new URL(resourceUrl)).host : ""

            if (resourceUrl && iconCache && currentHost.includes(webPageHost))
                icon.grabToImage((result) => iconCache.update(resourceUrl, result.image))
        }
    }
}
