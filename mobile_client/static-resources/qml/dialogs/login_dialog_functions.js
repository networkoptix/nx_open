function makeUrl(address, port, user, password) {
    return "http://" + user + ":" + password + "@" + address + ":" + port
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
        port.text = ""
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
}
