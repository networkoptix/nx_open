import QtQuick 2.0
import QtWebEngine 1.7
import QtWebChannel 1.0

WebEngineView {
    property bool enableInjections: false
    property list<WebEngineScript> injectScripts: [
        WebEngineScript {
            injectionPoint: WebEngineScript.DocumentCreation
            worldId: WebEngineScript.MainWorld
            name: "QWebChannel"
            sourceUrl: "qrc:///qtwebchannel/qwebchannel.js"
        },

        WebEngineScript {
            worldId: WebEngineScript.MainWorld
            injectionPoint: WebEngineScript.DocumentReady
            name: "NxObjectInjectorScript"
            sourceCode: "\
                window.channel = new QWebChannel(qt.webChannelTransport, function(channel) { \
                    Object.keys(channel.objects).forEach(function(e){ window[e] = channel.objects[e] }); \
                });"
        }
    ]

    id: webView
    url: ""
    focus: true
    onCertificateError: error.ignoreCertificateError()
    onJavaScriptConsoleMessage: console.log("level", level, "line", lineNumber, "source", sourceID, "message", message)

    signal loadingStatusChanged(int status)
    onLoadingChanged: loadingStatusChanged(loadRequest.status)

    webChannel: { return enableInjections ? nxWebChannel : null }
    userScripts: { return enableInjections ? injectScripts : [] }

    WebChannel {
        id: "nxWebChannel"
        objectName: "nxWebChannel"
    }
}
