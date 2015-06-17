import QtQuick 2.4
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "../controls"
import "../controls/style"
import "../icons"
import "QnLoginPage.js" as LoginFunctions

QnPage {
    id: loginPage

    property string newConnectionLabel: qsTr("New Connection")

    title: newConnectionLabel

    leftToolBarComponent: QnMenuBackIcon {

    }

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.leftMargin: dp(16)
        anchors.rightMargin: dp(16)
        contentWidth: width
        contentHeight: content.height

        Column {
            id: content
            width: parent.width
            spacing: dp(16)

            Rectangle {
                id: warningRect
                height: visible ? dp(48) : 0
                width: loginPage.width
                anchors.horizontalCenter: parent.horizontalCenter
                color: QnTheme.attentionColor
                visible: false

                Behavior on height { NumberAnimation { duration: 200 } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Incorrect login or password")
                    font.pixelSize: sp(16)
                    font.weight: Font.Bold
                    color: QnTheme.textColor
                }
            }

            Row {
                width: parent.width
                spacing: dp(16)

                QnTextField {
                    id: hostField
                    width: parent.width * 3 / 5
                    placeholderText: qsTr("Host")

                    text: "127.0.0.1"
                }

                QnTextField {
                    id: portField
                    width: parent.width * 2 / 5 - parent.spacing
                    placeholderText: qsTr("Port")

                    text: "7001"
                }
            }

            QnTextField {
                id: loginField
                width: parent.width
                placeholderText: qsTr("Login")

                text: "admin"
            }

            QnTextField {
                id: passwordField
                width: parent.width
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password

                text: "123"
            }

            Row {
                id: editButtons
                width: parent.width
                spacing: dp(16)

                QnButton {
                    id: saveButton
                    text: qsTr("Save")
                    width: parent.width * 3 / 5
                    style: QnButtonStyle {
                        color: "#2b383f"
                    }
                }

                QnButton {
                    id: deleteButton
                    text: qsTr("Delete")
                    width: parent.width * 2 / 5 - parent.spacing
                    style: QnButtonStyle {
                        color: "#d65029"
                    }
                }
            }

            QnButton {
                id: connectButton
                width: parent.width
                text: qsTr("Connect")

                onClicked: LoginFunctions.connectToServer(hostField.text, portField.text, loginField.text, passwordField.text)
            }
        }
    }
}
