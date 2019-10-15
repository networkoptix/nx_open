import QtQuick 2.0
import QtWebEngine 1.7
import QtWebChannel 1.0

WebEngineView {
    property bool enableInjections: false
    property bool enablePromises: false
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
            // Code that wraps each slot call into a function which returns slot
            // result as ES6 promise.
            property string wrapCode: !enablePromises ? "" : "\
                        if (typeof(obj.__objectSignals__) !== 'object') return; \
                        var qtFunctions = ['unwrapQObject', 'unwrapProperties', 'propertyUpdate', 'signalEmitted', 'deleteLater']; \
                        Object.keys(obj).forEach(function(funcName) { \
                            if (typeof(obj[funcName]) !== 'function') return; \
                            if (typeof(obj.__objectSignals__[funcName]) !== 'undefined') return; \
                            if (qtFunctions.includes(funcName)) return; \
                            var savedFuncName = '_original_' + funcName; \
                            obj[savedFuncName] = obj[funcName]; \
                            obj[funcName] = function() { \
                                var funcThis = this; \
                                var funcArgs = new Array(arguments.length); \
                                for (var i = 0; i < funcArgs.length; ++i) \
                                    funcArgs[i] = arguments[i]; \
                                return new Promise(function(resolve, reject) { \
                                    funcThis[savedFuncName].apply(funcThis, funcArgs.concat([function(result){ resolve(result); }])); \
                                }); \
                            }; \
                            console.log('Promise: ' + name + '.' + funcName);\
                        });"
            sourceCode: "\
                window.channel = new QWebChannel(qt.webChannelTransport, function(channel) { \
                    function wrapObject(name, obj) {"
                        + wrapCode +
                        "window[name] = obj; \
                    }; \
                    Object.keys(channel.objects).forEach(function(e){ wrapObject(e, channel.objects[e]); }); \
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

    profile.onDownloadRequested: workbench.requestDownload(download)
}
