function openDiscoveredSession(_sessionId, _host, _port, _systemName) {
    sideNavigation.hide()
    sideNavigation.activeSessionId = _sessionId

    stackView.push({
        item: Qt.resolvedUrl("items/QnLoginPage.qml"),
        properties: {
            title: _systemName,
            host: _host,
            port: _port,
            state: "Discovered",
            sideNavigationItem: sideNavigation
        }
    })
}

function openSavedSession(_sessionId, _host, _port, _login, _password, _systemName) {
    sideNavigation.hide()
    sideNavigation.activeSessionId = _sessionId

    stackView.push({
        item: Qt.resolvedUrl("items/QnLoginPage.qml"),
        properties: {
            title: _systemName,
            host: _host,
            port: _port,
            login: _login,
            password: _password,
            sessionId: _sessionId,
            state: "Saved"
        }
    })
}

function backToNewSession() {
    sideNavigation.activeSessionId = ""
    stackView.pop()
}
