.import QtQuick.Window 2.2 as QtWindow

function findRootItem(item) {
    while (item.parent)
        item = item.parent
    return item
}

function findRootChild(item, objectName) {
    item = findRootItem(item)

    var children = new Array(0)
    children.push(item)
    while (children.length > 0) {
        if (children[0].objectName == objectName)
            return children[0]

        for (var i in children[0].data)
            children.push(children[0].data[i])

        children.splice(0, 1)
    }

    return null
}

function isMobile() {
    return Qt.platform.os == "android" || Qt.platform.os == "ios" || Qt.platform.os == "winphone" || Qt.platform.os == "blackberry"
}

function openDiscoveredSession(_host, _port, _systemName) {
    sideNavigation.hide()
    sideNavigation.enabled = false
    menuBackButton.animateToBack()

    stackView.setSlideTransition()
    stackView.push({
        item: Qt.resolvedUrl("items/QnLoginPage.qml"),
        properties: {
            title: _systemName,
            host: _host,
            port: _port,
            sessionId: "",
            state: "Discovered"
        }
    })
}

function openSavedSession(_sessionId, _host, _port, _login, _password, _systemName) {
    sideNavigation.hide()
    sideNavigation.enabled = false
    menuBackButton.animateToBack()

    stackView.setSlideTransition()
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

function openFailedSession(_sessionId, _host, _port, _login, _password, _systemName, status, statusMessage) {
    var push = stackView.depth == 1

    var item = null

    var pageName = mainWindow.customConnection ? "newConnectionPage" : "loginPage"

    if (stackView.currentItem.objectName == pageName)
        item = stackView.currentItem

    sideNavigation.hide()

    if (!mainWindow.customConnection) {
        menuBackButton.animateToBack()
        sideNavigation.enabled = false
    }

    if (!item) {
        var pushList = []
        if (stackView.depth == 1)
            pushList.push(loginPageComponent)
        pushList.push({
            item: Qt.resolvedUrl("items/QnLoginPage.qml"),
            properties: {
                title: _systemName,
                host: _host,
                port: _port,
                login: _login,
                password: _password,
                sessionId: _sessionId,
                state: "FailedSaved"
            }
        })
        stackView.setSlideTransition()
        stackView.push(pushList)
        item = stackView.get(stackView.depth - 1)
    } else {
        if (_systemName && item.objectName != "newConnectionPage")
            item.title = _systemName
        item.host = _host
        item.port = _port
        item.login = _login
        item.password = _password
        item.sessionId = _sessionId
    }

    item.showWarning(status, statusMessage)
}

function gotoNewSession() {
    mainWindow.currentSessionId = ""
    mainWindow.currentSystemName = ""
    sideNavigation.enabled = true
    menuBackButton.animateToMenu()

    if (connectionManager.connected) {
        connectionManager.disconnectFromServer(true)
        return
    }

    var item = stackView.find(function(item, index) { return item.objectName === "newConnectionPage" })


    if (item) {
        stackView.setSlideTransition()
        stackView.pop(item)
    } else {
        stackView.setInvertedSlideTransition()
        stackView.push(loginPageComponent)
    }
}

function gotoResources() {
    var item = stackView.get(0)

    if (!item.searchActive) {
        menuBackButton.animateToMenu()
        sideNavigation.enabled = true
    }
    stackView.pop(item)
}

function gotoMainScreen() {
    if (activeResourceId)
        stackView.setScaleTransition()

    activeResourceId = ""

    toolBar.opacity = 1.0
    toolBar.backgroundOpacity = 1.0
    exitFullscreen()

    if (connectionManager.connected)
        gotoResources()
    else
        gotoNewSession()
}

function openMediaResource(uuid, xHint, yHint, initialScreenshot) {
    mainWindow.activeResourceId = uuid
    mainWindow.initialResourceScreenshot = initialScreenshot

    sideNavigation.hide()
    sideNavigation.enabled = false

    stackView.setScaleTransition(xHint, yHint)
    stackView.push(videoPlayerComponent)
}

function openSettings() {
    sideNavigation.hide()
    sideNavigation.enabled = false
    menuBackButton.animateToBack()
    stackView.setSlideTransition()
    stackView.push(settingsPageComponent)
}

function backPressed() {
    if (sideNavigation.open) {
        sideNavigation.hide()
        return true
    } else if (stackView.depth == 2) {
        // First stack item is always resources page.

        if (stackView.currentItem.objectName == "newConnectionPage")
            return false

        gotoMainScreen()
        return true
    }
    return false
}
