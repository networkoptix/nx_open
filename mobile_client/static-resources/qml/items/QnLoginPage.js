.import "../main.js" as Main

function makeUrl(host, port, login, password) {
    return "http://" + login + ":" + password + "@" + host + ":" + port
}

function updateSession(sessionId, host, port, login, password, systemName, moveTop) {
    return sideNavigation.savedSessionsModel.updateSession(sessionId, host, port, login, password, systemName, moveTop)
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
                    connectionManager.systemName,
                    true)
    }
}

function connectToServer(sessionId, host, port, login, password, customConnection) {
    mainWindow.currentHost = host
    mainWindow.currentPort = port
    mainWindow.currentLogin = login
    mainWindow.currentPasswrod = password
    mainWindow.currentSessionId = sessionId
    mainWindow.customConnection = customConnection ? true : false

    sideNavigation.hide()
    connectionManager.connectToServer(makeUrl(host, port, login, password))
}

function saveSession(sessionId, host, port, login, password, systemName) {
    updateSession(sessionId, host, port, login, password, systemName, false)

    Main.gotoMainScreen()

    if (connectionManager.connectionState == QnConnectionManager.Disconnected)
        sideNavigation.show()
}

function deleteSesion(sessionId) {
    sideNavigation.savedSessionsModel.deleteSession(sessionId)
    if (mainWindow.currentSessionId == sessionId)
        Main.gotoNewSession()
    else
        Main.gotoMainScreen()
}
