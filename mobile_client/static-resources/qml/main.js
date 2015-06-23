function openDiscoveredSession(_host, _port, _systemName) {
    mainWindow.currentSessionId = ""
    sideNavigation.hide()
    sideNavigation.enabled = false
    menuBackButton.animateToBack()

    stackView.push({
        item: Qt.resolvedUrl("items/QnLoginPage.qml"),
        properties: {
            title: _systemName,
            host: _host,
            port: _port,
            state: "Discovered"
        }
    })
}

function openSavedSession(_sessionId, _host, _port, _login, _password, _systemName) {
    mainWindow.currentSessionId = _sessionId
    sideNavigation.hide()
    sideNavigation.enabled = false
    menuBackButton.animateToBack()

    stackView.push({
        item: Qt.resolvedUrl("items/QnLoginPage.qml"),
        properties: {
            title: _systemName,
            host: _host,
            port: _port,
            login: _login,
            password: _password,
            state: "Saved"
        }
    })
}

function openFailedSession(_sessionId, _host, _port, _login, _password, _systemName, status, statusMessage) {
    mainWindow.currentSessionId = _sessionId
    sideNavigation.hide()
    sideNavigation.enabled = false
    menuBackButton.animateToBack()

    stackView.push({
        item: Qt.resolvedUrl("items/QnLoginPage.qml"),
        properties: {
            title: _systemName,
            host: _host,
            port: _port,
            login: _login,
            password: _password,
            state: "FailedSaved"
        }
    })

    var item = stackView.get(stackView.depth - 1)
    item.showWarning(status, statusMessage)
}

function gotoNewSession() {
    mainWindow.currentSessionId = ""
    sideNavigation.enabled = true

    if (connectionManager.connected) {
        connectionManager.disconnectFromServer(true)
        return
    }

    var item = stackView.find(function(item, index) { return item.objectName === "newConnectionPage" })

    if (item) {
        menuBackButton.animateToMenu()
        stackView.pop(item)
    } else {
        stackView.push(loginPageComponent)
    }
}

function gotoResources() {
    menuBackButton.animateToMenu()
    sideNavigation.enabled = true
    stackView.pop(stackView.get(0))
}

function gotoMainScreen() {
    if (connectionManager.connected)
        gotoResources()
    else
        gotoNewSession()
}

function openMediaResource(uuid) {
    mainWindow.activeResourceId = uuid
    sideNavigation.hide()
    sideNavigation.enabled = false
    menuBackButton.animateToBack()
    stackView.push(videoPlayerComponent)
}
