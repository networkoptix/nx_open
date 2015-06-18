.import "../main.js" as Main

var currentHost
var currentPort
var currentLogin
var currentPasswrod

function makeUrl(host, port, login, password) {
    return "http://" + login + ":" + password + "@" + host + ":" + port
}

function updateSession(host, port, login, password, systemName) {
    sideNavigation.savedSessionsModel.updateSession(host, port, login, password, systemName)
}

function clearCurrentSession() {
    currentPort = -1
}

function updateCurrentSession() {
    if (currentPort > 0)
        updateSession(currentHost, currentPort, currentLogin, currentPasswrod, connectionManager.systemName)
}

function connectToServer(host, port, login, password) {
    currentHost = host
    currentPort = port
    currentLogin = login
    currentPasswrod = password

    sideNavigation.hide()
    connectionManager.connectToServer(makeUrl(host, port, login, password))
}

function saveSession(host, port, login, password, systemName) {
    updateSession(host, port, login, password, systemName)
    Main.backToNewSession()
    sideNavigation.show()
}

function deleteSesion(sessionId) {
    sideNavigation.savedSessionsModel.deleteSession(sessionId)
    Main.backToNewSession()
    sideNavigation.show()
}
