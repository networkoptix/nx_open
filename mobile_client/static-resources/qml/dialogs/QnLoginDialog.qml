import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.2

import com.networkoptix.qml 1.0

import "../controls"

import "login_dialog_functions.js" as LoginDialogFunctions
import "../common_functions.js" as CommonFunctions

Item {
    /* backgorund */
    Rectangle {
        anchors.fill: parent
        color: colorTheme.color("login_background")
    }

    Image {
        id: logo

        anchors.horizontalCenter: parent.horizontalCenter
        y: (parent.height / 2 - height) / 2

        source: "/images/logo.png"
        fillMode: Image.PreserveAspectFit

        sourceSize.width: Math.min(parent.width, parent.height / 2)
        sourceSize.height: parent.height / 2
    }

    Column {
        y: parent.height / 2
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 2 / 3

        QnTextField {
            id: address
            placeholderText: qsTr("Server address")
            width: 200
            height: 25
        }
        QnTextField {
            id: port
            validator: IntValidator {
                bottom: 1
                top: 65535
            }
            leftPadding: portLabel.width + CommonFunctions.dp(8)

            Text {
                id: portLabel
                anchors.verticalCenter: parent.verticalCenter
                x: 0
                text: qsTr("Port")
                color: colorTheme.color("inputPlaceholderText")
            }

            text: "7001"
        }
        QnTextField {
            id: login
            placeholderText: qsTr("User name")
        }
        QnTextField {
            id: password
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
        }

        QnRoundButton {
            color: "#0096ff"
            icon: "/images/right.png"
            onClicked: connectionManager.connectToServer(LoginDialogFunctions.makeUrl(address.text, port.value, login.text, password.text))
        }
    }

//    Connections {
//        target: connectionManager
//    }
}
