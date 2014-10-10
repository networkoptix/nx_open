import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.2

import com.networkoptix.qml 1.0

import "../controls"

import "login_dialog_functions.js" as LoginDialogFunctions
import "../common_functions.js" as CommonFunctions

Item {
    id: loginDialog

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
        id: fieldsColumn

        y: parent.height / 2
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 2 / 3
        spacing: CommonFunctions.dp(8)

        QnTextField {
            id: address
            placeholderText: qsTr("Server address")
            width: parent.width
        }
        QnTextField {
            id: port
            leftPadding: portLabel.width + CommonFunctions.dp(8)
            width: parent.width

            Text {
                id: portLabel
                anchors.verticalCenter: parent.verticalCenter
                x: port.textPadding
                text: qsTr("Port")
                color: colorTheme.color("inputPlaceholderText")
                renderType: Text.NativeRendering
            }

            validator: IntValidator {
                bottom: 1
                top: 65535
            }
            text: "7001"
        }
        QnTextField {
            id: login
            placeholderText: qsTr("User name")
            width: parent.width
        }
        QnTextField {
            id: password
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            width: parent.width
        }

    }

    QnRoundButton {
        id: loginButton
        anchors {
            bottom: parent.bottom
            right: fieldsColumn.right
            bottomMargin: parent.width - (x + width)
        }
        color: "#0096ff"

        icon: "/images/right.png"
        onClicked: connectionManager.connectToServer(LoginDialogFunctions.makeUrl(address.text, port.text, login.text, password.text))
    }


//    Connections {
//        target: connectionManager
//    }
}
