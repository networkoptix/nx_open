var __currentAddress
var __currentPort
var __currentUser
var __currentPassword

function makeUrl(address, port, user, password) {
    return "http://" + user + ":" + password + "@" + address + ":" + port
}

function updateCurrent(address, port, user, password) {
    __currentAddress = address
    __currentPort = port
    __currentUser = user
    __currentPassword = password
}

function connectToServer(address, port, user, password) {
    updateCurrent(address, port, user, password)
    connectionManager.connectToServer(makeUrl(address, port, user, password))
}

function saveCurrentSession() {
    sessionsModel.updateSession(__currentAddress, __currentPort, __currentUser, __currentPassword, connectionManager.systemName)
}

function select(id) {
    if (sessionsList.selection.indexOf(id) == -1) {
        var tmp = sessionsList.selection
        tmp.push(id)
        sessionsList.selection = tmp
    }
}

function deleteSesions(sessions) {
    for (var i = 0; i < sessions.length; i++)
        sessionsModel.deleteSession(sessions[i])
}
