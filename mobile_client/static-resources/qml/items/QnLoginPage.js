.import "../main.js" as Main

function makeUrl(host, port, login, password) {
    return "http://" + login + ":" + password + "@" + host + ":" + port
}

function updateSession(sessionId, host, port, login, password, systemName) {
    return sideNavigation.savedSessionsModel.updateSession(sessionId, host, port, login, password, systemName)
}

function clearCurrentSession() {
    mainWindow.currentPort = -1
}

function saveCurrentSession() {
    if (mainWindow.currentPort > 0) {
        mainWindow.currentSessionId = updateSession(
                    mainWindow.currentSessionId,
                    mainWindow.currentHost, mainWindow.currentPort,
                    mainWindow.currentLogin, mainWindow.currentPasswrod,
                    connectionManager.systemName)
    }
}

function connectToServer(sessionId, host, port, login, password) {
    mainWindow.currentHost = host
    mainWindow.currentPort = port
    mainWindow.currentLogin = login
    mainWindow.currentPasswrod = password
    mainWindow.currentSessionId = sessionId

    sideNavigation.hide()
    connectionManager.connectToServer(makeUrl(host, port, login, password))
}

function saveSession(sessionId, host, port, login, password, systemName) {
    updateSession(sessionId, host, port, login, password, systemName)

    if (!connectionManager.connected)
        sideNavigation.show()
    Main.gotoMainScreen()
}

function deleteSesion(sessionId) {
    sideNavigation.savedSessionsModel.deleteSession(sessionId)
    Main.gotoNewSession()
    sideNavigation.show()
}
