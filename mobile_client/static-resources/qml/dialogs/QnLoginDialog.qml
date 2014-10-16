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

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    property int __currentIndex: -1

    QnContextSettings {
        id: settings
    }

    /* backgorund */
    Rectangle {
        anchors.fill: parent
        color: __syspal.window
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

    Item {
        id: savedSessions

        anchors.top: logo.bottom
        anchors.bottom: parent.bottom
        width: parent.width

        ListView {
            id: savedSessionsList

            anchors.centerIn: parent
            width: parent.width * 2 / 3
            height: parent.height / 3

            delegate: Item {
                width: savedSessionsList.width
                height: label.height + CommonFunctions.dp(8)

                Text {
                    id: label
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name
                    renderType: Text.NativeRendering
                    color: __syspal.windowText
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        __currentIndex = index
                        LoginDialogFunctions.updateUi(modelData)
                        loginDialog.state = "EDIT"
                    }
                }
            }
        }
    }

    Item {
        id: newSession

        anchors.top: logo.bottom
        anchors.bottom: parent.bottom
        anchors.left: savedSessions.right
        width: parent.width

        Column {
            id: fieldsColumn


            anchors.centerIn: parent
            width: parent.width * 2 / 3
            spacing: CommonFunctions.dp(8)

            QnTextField {
                id: name
                placeholderText: qsTr("Session name")
                width: parent.width
            }

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
    }

    QnRoundButton {
        id: loginButton
        anchors {
            bottom: parent.bottom
            right: parent.right
            bottomMargin: parent.width - (x + width)
            rightMargin: parent.width / 6
        }
        color: "#0096ff"

        icon: "/images/right.png"
        onClicked: {
            LoginDialogFunctions.saveCurrentSession()
            connectionManager.connectToServer(LoginDialogFunctions.makeUrl(address.text, port.text, login.text, password.text))
        }
    }

    states: [
        State {
            name: "CHOOSE"
            PropertyChanges {
                target: savedSessions
                x: 0
            }
        },
        State {
            name: "EDIT"
            PropertyChanges {
                target: savedSessions
                x: -loginDialog.width
            }
        }
    ]

    transitions: [
        Transition {
            from: "CHOOSE"
            to: "EDIT"
            NumberAnimation {
                target: savedSessions
                property: "x"
                easing.type: Easing.OutQuad
                duration: 200
            }
        }
    ]

    Component.onCompleted: {
        savedSessionsList.model = settings.savedSessions()
        state = savedSessionsList.count > 0 ? "CHOOSE" : "EDIT"
    }
}
