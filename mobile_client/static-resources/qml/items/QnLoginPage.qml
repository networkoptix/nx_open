import QtQuick 2.4
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "../controls"
import "../controls/style"
import "../items"
import "../main.js" as Main
import "QnLoginPage.js" as LoginFunctions

QnPage {
    id: loginPage

    property alias host: hostField.text
    property alias port: portField.text
    property alias login: loginField.text
    property alias password: passwordField.text
    property string sessionId: mainWindow.currentSessionId

    property string newConnectionLabel: qsTr("New Connection")

    title: newConnectionLabel

    property bool _authError: false
    property bool _serverError: false
    property bool _showWarning: false

    property int _warningAnimationDuration: 500

    Connections {
        target: menuBackButton
        onClicked: {
            if (state != "New")
                Main.gotoMainScreen()
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        leftMargin: dp(16)
        rightMargin: dp(16)
        contentWidth: width
        contentHeight: content.height
        clip: true

        Column {
            id: content
            width: parent.width - flickable.leftMargin - flickable.rightMargin
            spacing: dp(16)

            Rectangle {
                id: warningRect
                height: _showWarning ? dp(48) : 0
                width: loginPage.width
                anchors.horizontalCenter: parent.horizontalCenter
                color: QnTheme.attentionBackground
                clip: true

                Behavior on height { NumberAnimation { duration: _warningAnimationDuration; easing.type: Easing.OutCubic } }

                Text {
                    id: warningText
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.top
                    anchors.verticalCenterOffset: dp(24)
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
                    warning: _serverError
                    onTextChanged: loginPage.removeWarnings()
                }

                QnTextField {
                    id: portField
                    width: parent.width * 2 / 5 - parent.spacing
                    placeholderText: qsTr("Port")
                    warning: _serverError
                    onTextChanged: loginPage.removeWarnings()
                }
            }

            QnTextField {
                id: loginField
                width: parent.width
                placeholderText: qsTr("Login")
                warning: _authError
                onTextChanged: loginPage.removeWarnings()
            }

            QnTextField {
                id: passwordField
                width: parent.width
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                warning: _authError
                onTextChanged: loginPage.removeWarnings()
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

                    onClicked: LoginFunctions.saveSession(sessionId, hostField.text, portField.text, loginField.text, passwordField.text, title)
                }

                QnButton {
                    id: deleteButton
                    text: qsTr("Delete")
                    width: parent.width * 2 / 5 - parent.spacing
                    color: QnTheme.buttonAttentionBackground

                    onClicked: LoginFunctions.deleteSesion(sessionId)
                }
            }

            QnButton {
                id: connectButton
                width: parent.width
                text: qsTr("Connect")
                color: QnTheme.buttonAccentBackground

                onClicked: {
                    loginPage.removeWarnings()

                    if (_showWarning) {
                        _showWarning = false
                        delayedLoginTimer.start()
                    } else {
                        loginWithCurrentFields()
                    }
                }
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
                width: parent.width

                model: QnLoginSessionsModel {
                    id: discoveredSessionsModel
                    displayMode: QnLoginSessionsModel.ShowDiscovered
                }

                QnDiscoveredSessionItem {
                    systemName: model.systemName
                    host: model.address
                    port: model.port
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
            StateChangeScript {
                script: {
                    loginField.forceActiveFocus()
                }
            }
        },
        State {
            name: "Saved"
            PropertyChanges {
                target: editButtons
                visible: true
            }
            PropertyChanges {
                target: connectButton
                visible: false
            }
        }
    ]

    Connections {
        target: connectionManager

        onConnectionFailed: {
            if (!activePage)
                return

            warningText.text = statusMessage
            if (status == QnConnectionManager.Unauthorized)
                _authError = true
            else
                _serverError = true
            _showWarning = true
        }
    }

    Timer {
        id: delayedLoginTimer
        interval: _warningAnimationDuration
        running: false
        repeat: false
        onTriggered: loginWithCurrentFields()
    }

    function removeWarnings() {
        _authError = false
        _serverError = false
    }

    function loginWithCurrentFields() {
        LoginFunctions.connectToServer("", hostField.text, portField.text, loginField.text, passwordField.text)
    }
}
