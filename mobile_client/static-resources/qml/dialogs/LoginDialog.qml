import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

import com.networkoptix.qml 1.0

import "login_dialog_functions.js" as LoginDialogFunctions

Item {
    /* backgorund */
    Rectangle {
        anchors.fill: parent
        color: colorTheme.color("login_background")
    }

    Image {
        id: logo
        source: "qrc:///logo.png"
        fillMode: Image.PreserveAspectFit
        sourceSize.width: parent.width
        sourceSize.height: parent.width
    }

    ColumnLayout {
        anchors { top: logo.bottom; bottom: parent.bottom }
        width: parent.width

        TextField {
            id: address
            placeholderText: qsTr("Server address")
        }
        SpinBox {
            id: port
            minimumValue: 1
            maximumValue: 65535
            value: 7001
        }
        TextField {
            id: login
            placeholderText: qsTr("User name")
        }
        TextField {
            id: password
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
        }

        Button {
            text: qsTr("Connect")
            onClicked: connectionManager.connectToServer(LoginDialogFunctions.makeUrl(address.text, port.value, login.text, password.text))
        }
    }

//    Connections {
//        target: connectionManager
//    }
}
