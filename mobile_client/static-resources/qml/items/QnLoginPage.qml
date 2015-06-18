import QtQuick 2.4
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "../controls"
import "../controls/style"
import "QnLoginPage.js" as LoginFunctions

QnPage {
    id: loginPage

    property alias host: hostField.text
    property alias port: portField.text
    property alias login: loginField.text
    property alias password: passwordField.text

    property string newConnectionLabel: qsTr("New Connection")

    signal openDiscoveredSessionRequested(string host, int port, string systemName)

    title: newConnectionLabel

    leftToolBarComponent: QnMenuBackButton {
        navigationDrawer: navDrawer
    }

    QnNavigationDrawer {
        id: navDrawer
    }

    QnLoginSessionsModel {
        id: discoveredSessionsModel
        displayMode: QnLoginSessionsModel.ShowDiscovered
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.leftMargin: dp(16)
        anchors.rightMargin: dp(16)
        contentWidth: width
        contentHeight: content.height
        clip: true

        Column {
            id: content
            width: parent.width
            spacing: dp(16)

            Rectangle {
                id: warningRect
                height: visible ? dp(48) : 0
                width: loginPage.width
                anchors.horizontalCenter: parent.horizontalCenter
                color: QnTheme.attentionBackground
                visible: false

                Behavior on height { NumberAnimation { duration: 200 } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Incorrect login or password")
                    font.pixelSize: sp(16)
                    font.weight: Font.Bold
                    color: QnTheme.windowText
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
                visible: false

                QnButton {
                    id: saveButton
                    text: qsTr("Save")
                    width: parent.width * 3 / 5
                }

                QnButton {
                    id: deleteButton
                    text: qsTr("Delete")
                    width: parent.width * 2 / 5 - parent.spacing
                    style: QnButtonStyle {
                        color: QnTheme.buttonAttentionBackground
                    }
                }
            }

            QnButton {
                id: connectButton
                width: parent.width
                text: qsTr("Connect")
                style: QnButtonStyle {
                    color: QnTheme.buttonAccentBackground
                }

                onClicked: LoginFunctions.connectToServer(hostField.text, portField.text, loginField.text, passwordField.text)
            }

            Loader {
                id: discoveredSessionsLoader
                width: parent.width
            }
        }
    }

    Component {
        id: discoveredSessionsList

        Column {
            spacing: dp(1)

            Text {
                height: dp(48)
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Auto-discovered systems")
                color: QnTheme.listSectionText
                font.pixelSize: sp(14)
                font.weight: Font.DemiBold
            }

            Repeater {
                id: discoveredSessionRepeater
                model: discoveredSessionsModel
                width: parent.width

                Rectangle {
                    width: parent.width
                    height: dp(72)
                    color: QnTheme.sessionItemBackground
                    radius: dp(2)

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: dp(8)
                        x: dp(16)
                        width: parent.width - 2 * x

                        Text {
                            text: systemName
                            color: QnTheme.listText
                            font.pixelSize: sp(16)
                            font.weight: Font.Bold
                        }

                        Text {
                            text: address + ":" + port
                            color: QnTheme.listSubText
                            font.pixelSize: sp(14)
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: loginPage.openDiscoveredSessionRequested(address, port, systemName)
                    }
                }
            }

            Item {
                /*padding*/
                width: parent.width
                height: dp(16)
            }
        }
    }

    state: "New"

    states: [
        State {
            name: "New"
            PropertyChanges {
                target: discoveredSessionsLoader
                sourceComponent: discoveredSessionsList
            }
        },
        State {
            name: "Discovered"
        },
        State {
            name: "Saved"
            PropertyChanges {
                target: editButtons
                visible: true
            }
        }
    ]
}
