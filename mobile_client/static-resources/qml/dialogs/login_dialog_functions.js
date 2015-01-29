function makeUrl(address, port, user, password) {
    return "http://" + user + ":" + password + "@" + address + ":" + port
}

function connectToServer(_address, _port, _user, _password) {
    connectionManager.connectToServer(makeUrl(_address, _port, _user, _password))
}

function saveCurrentSession() {
    sessionsModel.updateSession(sessionEditDialog.address, sessionEditDialog.port, sessionEditDialog.user, sessionEditDialog.password, connectionManager.systemName)
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
