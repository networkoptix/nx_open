function makeUrl(address, port, user, password) {
    return "http://" + user + ":" + password + "@" + address + ":" + port
}

function connectToServer() {
    saveCurrentSession()
    connectionManager.connectToServer(makeUrl(address.text, port.text, login.text, password.text))
}

function updateUi(modelData) {
    if (modelData) {
        name.text = modelData.name
        address.text = modelData.address
        port.text = modelData.port
        login.text = modelData.login
        password.text = modelData.password
    } else {
        name.text = ""
        address.text = ""
        port.text = "7001"
        login.text = ""
        password.text = ""
    }
}

function saveCurrentSession() {
    settings.saveSession({
        name: name.text,
        address: address.text,
        port: port.text,
        login: login.text,
        password: password.text
    }, __currentIndex)
    savedSessionsList.model = settings.savedSessions()
    __currentIndex = 0
}

function select(index) {
    if (savedSessionsList.selection.indexOf(index) == -1) {
        savedSessionsList.selection = savedSessionsList.selection.concat([index])
        titleBar.state = "SELECT"
    }
}

function clearSelection() {
    savedSessionsList.selection = []
}

function isSelected(index) {
    return savedSessionsList.selection.indexOf(index) != -1
}

function deleteSelected() {
    var selection = savedSessionsList.selection.sort()
    savedSessionsList.selection = []
    for (var i = selection.length - 1; i >= 0; i--)
        settings.removeSession(selection[i])
    savedSessionsList.model = settings.savedSessions()
    titleBar.state = "HIDDEN"
}
