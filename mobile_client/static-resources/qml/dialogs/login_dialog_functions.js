function makeUrl(address, port, user, password) {
    return "http://" + user + ":" + password + "@" + address + ":" + port
}

function connectToServer(_address, _port, _user, _password) {
    connectionManager.connectToServer(makeUrl(_address, _port, _user, _password))
}

function editSession(_systemName, _address, _port, _user, _password) {
    systemNameLabel.text = _systemName
    address.text = _address
    port.text = _port
    login.text = _user ? _user : "admin"
    password.text = _password

    login.forceActiveFocus()
}

function newSession() {
    systemNameLabel.text = qsTr("New session")
    address.text = ""
    port.text = 7001
    login.text = "admin"
    password.text = ""

    address.forceActiveFocus()
}

function saveCurrentSession() {
    sessionsModel.updateSession(address.text, port.text, login.text, password.text, systemNameLabel.text)
}

function select(id) {
    if (savedSessionsList.selection.indexOf(id) == -1) {
        savedSessionsList.selection.push(id)
        titleBar.state = "SELECT"
    }
}

function clearSelection() {
    savedSessionsList.selection = []
}

function isSelected(id) {
    return savedSessionsList.selection.indexOf(id) != -1
}

function deleteSelected() {
    var selection = savedSessionsList.selection
    savedSessionsList.selection = []
    for (var i = selection.length - 1; i >= 0; i--)
        sessionsModel.deleteSession(selection[i])
    titleBar.state = "HIDDEN"
}
