import QtQuick 2.6
import QtWebView 1.0
import Nx 1.0
import Nx.Controls 1.0

PageBase
{
    id: liteClientWelcomeScreen
    objectName: "liteClientWelcomeScreen"

    WebView
    {
        id: webView

        anchors.fill: parent
    }

    Component.onCompleted:
    {
        var baseUrl = NxGlobals.url(getInitialUrl())

        var url = "http://" + baseUrl.address()
            + "/static/index.html"
            + "?clientWebSocket=" + encodeURIComponent(getWebSocketUrl())

        webView.url = url
    }
}
