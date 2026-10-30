// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtWebEngine

import Nx.Controls

WebEngineView
{
    id: view

    property alias result: testObject.result
    property var ready: null
    readonly property string kEmptyWebPage: "<html></html>"

    enableInjections: true

    // Only a plain object is exposed to the page: the web channel also tracks the objects
    // referenced by the properties of the exposed one, and the view items must not be among them.
    QtObject
    {
        id: testObject

        property var result: null
    }

    Component.onCompleted:
    {
        webView.loadHtml(kEmptyWebPage, "https://localhost/") //< Some base url is required.
        webView.webChannel.registerObjects({"test": testObject})
    }

    onLoadingStatusChanged: (status) => { ready = status == WebEngineView.LoadSucceededStatus }

    function checkWritingClipboard()
    {
        webView.runJavaScript("navigator.clipboard.write([new ClipboardItem({'text/plain': ''})])"
            + ".then(() => test.result = true)"
            + ".catch(() => test.result = false)");
    }

    function checkReadingClipboard()
    {
        webView.runJavaScript("navigator.clipboard.read()"
            + ".then(() => test.result = true)"
            + ".catch(() => test.result = false)");
    }

    function checkWritingTextClipboard()
    {
        webView.runJavaScript("navigator.clipboard.writeText('example')"
            + ".then(() => test.result = true)"
            + ".catch(() => test.result = false)");
    }

    function checkReadingTextClipboard()
    {
        webView.runJavaScript("navigator.clipboard.readText()"
            + ".then(() => test.result = true)"
            + ".catch(() => test.result = false)");
    }

    function checkPasteCommand()
    {
        webView.runJavaScript("test.result = document.execCommand('paste')")
    }
}
