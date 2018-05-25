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
        var url = "http://" + getInitialUrl().displayAddress()
            + "/static/index.html"
            + "?clientWebSocket=" + encodeURIComponent(getWebSocketUrl())

        webView.url = url
    }
}
