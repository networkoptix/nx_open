import QtQuick 2.2
import Material 0.1

Dialog {
    property alias address: addressField.text
    property alias port: portField.text
    property alias user: userField.text
    property alias password: passwordField.text

    positiveBtnText: qsTr("Connect")

    Column {
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
            input.echoMode: TextInput.Password
        }
    }

    function openNewSession() {
        title = qsTr("New session")
        address = ""
        port = 7001
        user = "admin"
        password = ""
        addressField.forceActiveFocus()

        open()
    }

    function openExistingSession(_systemName, _address, _port, _user, _password) {
        title = _systemName
        address = _address
        port = _port
        user = _user ? _user : "admin"
        password = _password
        userField.input.selectAll()
        userField.forceActiveFocus()

        open()
    }
}
