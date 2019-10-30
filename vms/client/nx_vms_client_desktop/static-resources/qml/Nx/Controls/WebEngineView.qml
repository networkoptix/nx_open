import QtQuick 2.0
import QtWebEngine 1.7
import QtWebChannel 1.0

WebEngineView
{
    id: webView

    property bool enableInjections: false
    property bool enablePromises: false
    property list<WebEngineScript> injectScripts: [
        WebEngineScript
        {
            injectionPoint: WebEngineScript.DocumentCreation
            worldId: WebEngineScript.MainWorld
            name: "QWebChannel"
            sourceUrl: "qrc:///qtwebchannel/qwebchannel.js"
        },

        WebEngineScript
        {
            worldId: WebEngineScript.MainWorld
            injectionPoint: WebEngineScript.DocumentReady
            name: "NxObjectInjectorScript"
            sourceUrl: "qrc:///qml/Nx/Controls/web_engine_inject" + (enablePromises ? "_promises.js" : ".js")
        }
    ]

    url: ""
    focus: true
    onCertificateError: error.ignoreCertificateError()
    webChannel: enableInjections ? nxWebChannel : null
    userScripts: enableInjections ? injectScripts : []

    signal loadingStatusChanged(int status)
    onLoadingChanged: loadingStatusChanged(loadRequest.status)
    onJavaScriptDialogRequested: workbench.requestJavaScriptDialog(request)
    onAuthenticationDialogRequested: workbench.requestAuthenticationDialog(request)
    onFileDialogRequested: workbench.requestFileDialog(request)
    profile.onDownloadRequested: workbench.requestDownload(download)
    onContextMenuRequested: function(request) { request.accepted = true }

    function triggerAction(action) { triggerWebAction(action) }

    WebChannel
    {
        id: nxWebChannel
        objectName: "nxWebChannel"
    }
}
