import QtQuick 2.2
import Material 0.1

Dialog {
    property alias address: addressField.text
    property alias port: portField.text
    property alias user: userField.text
    property alias password: passwordField.text

    positiveButtonText: qsTr("Connect")

    TextField {
        id: addressField
        placeholderText: qsTr("Address")
        floatingLabel: true
    }

    TextField {
        id: portField
        placeholderText: qsTr("Port")
        floatingLabel: true
    }

    TextField {
        id: userField
        placeholderText: qsTr("User")
        floatingLabel: true
    }

    TextField {
        id: passwordField
        placeholderText: qsTr("Password")
        floatingLabel: true
        echoMode: TextInput.Password
    }

    function openNewSession() {
        title = qsTr("New session")
        address = ""
        port = 7001
        user = "admin"
        password = ""
        addressField.forceActiveFocus()

        show()
    }

    function openExistingSession(_systemName, _address, _port, _user, _password) {
        title = _systemName
        address = _address
        port = _port
        user = _user ? _user : "admin"
        password = _password
        userField.selectAll()
        userField.forceActiveFocus()

        show()
    }
}
