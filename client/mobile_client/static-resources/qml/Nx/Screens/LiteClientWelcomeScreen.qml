import QtQuick 2.6
import QtWebView 1.0
import Nx 1.0
import Nx.Controls 1.0

PageBase
{
    id: liteClientWelcomeScreen

    WebView
    {
        id: webView

        anchors.fill: parent
    }

    Component.onCompleted:
    {
        var baseUrl = Nx.url(getInitialUrl())

        var url = baseUrl.scheme() + "://" + baseUrl.address()
            + "?clientWebSocket=" + encodeURIComponent(getWebSocketUrl())

        webView.url = url
    }
}
