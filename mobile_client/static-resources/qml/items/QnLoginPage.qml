import QtQuick 2.4
import QtQuick.Controls 1.2

import "../controls"
import "QnLoginPage.js" as LoginFunctions

QnPage {
    id: loginPage

    property string newConnectionLabel: qsTr("New Connection")

    title: newConnectionLabel

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
                width: parent.width
                height: visible ? dp(48) : 0
                visible: false
            }

            Row {
                width: parent.width
                spacing: dp(16)

                TextField {
                    id: hostField
                    width: parent.width * 3 / 5
                    placeholderText: qsTr("Host")

                    text: "127.0.0.1"
                }

                TextField {
                    id: portField
                    width: parent.width * 2 / 5 - parent.spacing
                    placeholderText: qsTr("Port")

                    text: "7001"
                }
            }

            TextField {
                id: loginField
                width: parent.width
                placeholderText: qsTr("Login")

                text: "admin"
            }

            TextField {
                id: passwordField
                width: parent.width
                placeholderText: qsTr("Password")

                text: "123"
            }

            Row {
                id: editButtons
                width: parent.width
                spacing: dp(16)

                Button {
                    id: saveButton
                    text: qsTr("Save")
                    width: parent.width * 3 / 5
                }

                Button {
                    id: deleteButton
                    text: qsTr("Delete")
                    width: parent.width * 2 / 5 - parent.spacing
                }
            }

            Button {
                id: connectButton
                width: parent.width
                text: qsTr("Connect")

                onClicked: LoginFunctions.connectToServer(hostField.text, portField.text, loginField.text, passwordField.text)
            }
        }
    }
}
