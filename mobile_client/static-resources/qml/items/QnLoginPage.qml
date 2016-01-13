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
    property string sessionId: ""

    property string newConnectionLabel: qsTr("New Connection")

    property var oldClientOfferDialog: oldClientOfferDialog

    title: newConnectionLabel

    property bool _authError: false
    property bool _serverError: false
    property bool _showWarning: false

    property int _warningAnimationDuration: 500

    objectName: "loginPage"

    Connections {
        target: menuBackButton
        onClicked: {
            if (state != "New")
                Main.gotoMainScreen()
        }
    }

    Item {
        id: warning

        width: parent.width
        height: warningRect.height + dp(8)

        Rectangle {
            id: warningRect

            width: parent.width
            height: _showWarning ? dp(40) : 0
            clip: true
            anchors.horizontalCenter: parent.horizontalCenter

            color: QnTheme.attentionBackground

            Behavior on height { NumberAnimation { duration: _warningAnimationDuration; easing.type: Easing.OutCubic } }

            Text {
                id: warningText
                anchors.horizontalCenter: parent.horizontalCenter
                // Two lines below are the hack to prevent text from moving when the header changes its size
                anchors.verticalCenter: parent.bottom
                anchors.verticalCenterOffset: -dp(20)
                font.pixelSize: sp(16)
                font.weight: Font.DemiBold
                color: QnTheme.windowText
            }
        }
    }

    QnFlickable {
        id: flickable
        anchors.fill: parent
        anchors.topMargin: warning.height
        topMargin: dp(16)
        leftMargin: dp(16)
        rightMargin: dp(16)
        bottomMargin: dp(16)
        contentWidth: width
        contentHeight: content.height
        clip: true

        Column {
            id: content
            width: parent.width - flickable.leftMargin - flickable.rightMargin
            spacing: dp(24)

            Row {
                width: parent.width

                QnTextField {
                    id: hostField
                    width: parent.width * 3 / 5
                    placeholderText: qsTr("Host")
                    showError: _serverError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    KeyNavigation.tab: loginField
                    onAccepted: KeyNavigation.tab.forceActiveFocus()
                }

                QnTextField {
                    id: portField
                    width: parent.width * 2 / 5 - parent.spacing
                    placeholderText: qsTr("Port (optional)")
                    showError: _serverError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 32767 }
                    KeyNavigation.tab: loginField
                    onAccepted: KeyNavigation.tab.forceActiveFocus()
                }
            }

            Column {
                width: parent.width

                QnTextField {
                    id: loginField
                    width: parent.width
                    placeholderText: qsTr("Login")
                    showError: _authError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                    KeyNavigation.tab: passwordField
                    onAccepted: KeyNavigation.tab.forceActiveFocus()
                }

                QnTextField {
                    id: passwordField
                    width: parent.width
                    placeholderText: qsTr("Password")
                    echoMode: TextInput.Password
                    passwordMaskDelay: 1500
                    showError: _authError
                    onTextChanged: loginPage.removeWarnings()
                    selectionAllowed: false
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData | Qt.ImhHiddenText
                    KeyNavigation.tab: hostField
                    onAccepted: connect()
                    Component.onCompleted: {
                        if (Qt.platform.os == "android")
                            passwordCharacter = "\u2022"
                    }
                }
            }

            Row {
                id: editButtons
                width: parent.width
                spacing: dp(8)
                visible: false

                QnButton {
                    id: saveButton
                    text: qsTr("Save")
                    width: parent.width * 3 / 5

                    onClicked: LoginFunctions.saveSession(sessionId, hostField.text, actualPort(), loginField.text, passwordField.text, title)
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
                textColor: QnTheme.buttonAccentText

                onClicked: connect()
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
                height: dp(40)
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Auto-discovered systems")
                color: QnTheme.listSectionText
                font.pixelSize: sp(16)
            }

            Repeater {
                id: discoveredSessionRepeater
                width: parent.width

                model: QnDiscoveredSessionsModel {
                    id: discoveredSessionsModel
                }

                QnDiscoveredSessionItem {
                    systemName: model.systemName
                    host: model.address
                    port: model.port
                    version: model.serverVersion
                }
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
        },
        State {
            name: "FailedSaved"
            PropertyChanges {
                target: editButtons
                visible: true
            }
        }
    ]

    Connections {
        target: connectionManager

        onConnectionFailed: {
            if (!activePage)
                return

            showWarning(status, infoParameter)
        }
    }

    Timer {
        id: delayedLoginTimer
        interval: _warningAnimationDuration
        running: false
        repeat: false
        onTriggered: loginWithCurrentFields()
    }

    QnOldClientOfferDialog {
        id: oldClientOfferDialog
    }

    function connect() {
        loginPage.removeWarnings()
        connectButton.forceActiveFocus()

        if (_showWarning) {
            _showWarning = false
            delayedLoginTimer.start()
        } else {
            loginWithCurrentFields()
        }
    }

    function showWarning(status, infoParameter) {
        var message = ""
        if (status == QnConnectionManager.Unauthorized)
            message = qsTr("Incorrect login or password")
        else if (status == QnConnectionManager.NetworkError)
            message = qsTr("Server or network is not available")
        else if (status == QnConnectionManager.InvalidServer)
            message = qsTr("Incompatible server")
        else if (status == QnConnectionManager.InvalidVersion)
            message = qsTr("Incompatible server version %1").arg(infoParameter)

        warningText.text = message
        if (status === QnConnectionManager.Unauthorized)
            _authError = true
        else
            _serverError = true
        _showWarning = true

        if (status == QnConnectionManager.InvalidVersion)
            oldClientOfferDialog.show()
    }

    function removeWarnings() {
        _authError = false
        _serverError = false
    }

    function loginWithCurrentFields() {
        var sessionId = (state == "FailedSaved" ? loginPage.sessionId : "")
        var customConnection = (loginPage.objectName == "newConnectionPage")
        LoginFunctions.connectToServer(sessionId, hostField.text, actualPort(), loginField.text, passwordField.text, customConnection)
    }

    function actualPort() {
        return portField.text ? portField.text : connectionManager.defaultServerPort()
    }

    focus: true

    Keys.onReleased: {
        if (Main.keyIsBack(event.key)) {
            if (Main.backPressed())
                event.accepted = true
        }
    }
}
