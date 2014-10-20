import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.2

import com.networkoptix.qml 1.0

import "../controls"

import "login_dialog_functions.js" as LoginDialogFunctions
import "../common_functions.js" as CommonFunctions

FocusScope {
    id: loginDialog

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    property int __currentIndex: -1
    property bool __animated: false

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
        id: actionBar

        width: parent.width
        height: CommonFunctions.dp(56)

        QnIconButton {
            id: backButton
            icon: "/images/back.png"
            x: CommonFunctions.dp(16)
            anchors.verticalCenter: parent.verticalCenter

            onClicked: {
                loginDialog.state = "CHOOSE"
            }
        }

        Row {
            anchors {
                right: parent.right
                rightMargin: CommonFunctions.dp(16)
                verticalCenter: parent.verticalCenter
            }

            QnIconButton {
                id: deleteButton
                icon: "/images/delete.png"

                onClicked: {
                    settings.removeSession(__currentIndex)
                    savedSessionsList.model = settings.savedSessions()
                    loginDialog.state = "CHOOSE"
                }
            }
        }

        states: [
            State {
                name: "HIDDEN"
                PropertyChanges {
                    target: actionBar
                    y: -actionBar.height
                }
            },
            State {
                name: "SELECT"
                PropertyChanges {
                    target: actionBar
                    y: 0
                }
            },
            State {
                name: "EDIT"
                PropertyChanges {
                    target: actionBar
                    y: 0
                }
            }
        ]
    }

    Item {
        id: savedSessions

        anchors.top: logo.bottom
        anchors.bottom: parent.bottom
        width: parent.width

        Text {
            anchors {
                bottom: savedSessionsList.top
                horizontalCenter: parent.horizontalCenter
                bottomMargin: CommonFunctions.dp(16)
            }

            color: __syspal.windowText
            text: qsTr("Saved sessions")
            renderType: Text.NativeRendering
            font.pixelSize: CommonFunctions.dp(40)
        }

        ListView {
            id: savedSessionsList

            anchors.centerIn: parent
            width: parent.width * 2 / 3
            height: parent.height / 3
            spacing: CommonFunctions.dp(8)
            boundsBehavior: ListView.StopAtBounds

            delegate: Item {
                property bool selected: false

                width: savedSessionsList.width
                height: label.height + CommonFunctions.dp(16)

                Rectangle {
                    anchors.fill: parent

                    color: "#303030"
                    radius: CommonFunctions.dp(2)
                }

                Text {
                    id: label
                    x: CommonFunctions.dp(8)
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
                    onPressAndHold: {

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
                onAccepted: address.focus = true
            }

            QnTextField {
                id: address
                placeholderText: qsTr("Server address")
                width: parent.width
                onAccepted: port.focus = true
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

                onAccepted: login.focus = true
            }

            QnTextField {
                id: login
                placeholderText: qsTr("User name")
                width: parent.width
                onAccepted: password.focus = true
            }

            QnTextField {
                id: password
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                width: parent.width
                onAccepted: LoginDialogFunctions.connectToServer()
            }

        }
    }

    QnRoundButton {
        id: addCancelButton
        anchors {
            bottom: parent.bottom
            bottomMargin: parent.width / 6
        }

        x: parent.width * 5 / 6 - width
        rotation: loginDialog.state == "CHOOSE" ? 0 : 45
        color: "#0096ff"
        icon: "/images/plus.png"

        Behavior on rotation { enabled: __animated; NumberAnimation { duration: 200 } }

        onClicked: {
            if (loginDialog.state == "CHOOSE") {
                LoginDialogFunctions.updateUi(null)
                __currentIndex = savedSessionsList.count
                loginDialog.state = "EDIT"
            } else {
                __currentIndex = -1
                loginDialog.state = "CHOOSE"
            }
        }
    }

    QnRoundButton {
        id: loginButton
        anchors {
            bottom: parent.bottom
            bottomMargin: parent.width / 6
        }
        x: parent.width
        color: "#0096ff"
        icon: "/images/forward.png"

        onClicked: LoginDialogFunctions.connectToServer()
    }

    states: [
        State {
            name: "CHOOSE"
            PropertyChanges {
                target: savedSessions
                x: 0
            }
            PropertyChanges {
                target: loginButton
                x: loginDialog.width
            }
            PropertyChanges {
                target: addCancelButton
                x: loginDialog.width * 5 / 6 - addCancelButton.width
            }
            PropertyChanges {
                target: actionBar
                state: "HIDDEN"
            }
        },
        State {
            name: "EDIT"
            PropertyChanges {
                target: savedSessions
                x: -loginDialog.width
            }
            PropertyChanges {
                target: loginButton
                x: loginDialog.width * 5 / 6 - loginButton.width
            }
            PropertyChanges {
                target: addCancelButton
                x: loginDialog.width / 6
            }
            PropertyChanges {
                target: actionBar
                state: "EDIT"
            }
        }
    ]

    transitions: [
        Transition {
            from: "CHOOSE"
            to: "EDIT"
            enabled: __animated
            NumberAnimation {
                target: savedSessions
                property: "x"
                easing.type: Easing.OutQuad
                duration: 200
            }
            NumberAnimation {
                targets: [loginButton, addCancelButton]
                properties: "x"
                easing.type: Easing.OutQuad
                duration: 200
            }
        },
        Transition {
            from: "EDIT"
            to: "CHOOSE"
            enabled: __animated
            NumberAnimation {
                target: savedSessions
                property: "x"
                easing.type: Easing.OutQuad
                duration: 200
            }
            NumberAnimation {
                targets: [loginButton, addCancelButton]
                properties: "x"
                easing.type: Easing.OutQuad
                duration: 200
            }
        }
    ]

    Keys.onReleased: {
        if (event.key == Qt.Key_Back) {
            if (loginDialog.state == "CHOOSE")
                Qt.quit()
            else
                loginDialog.state = "CHOOSE"
        }
    }

    Component.onCompleted: {
        savedSessionsList.model = settings.savedSessions()
        state = savedSessionsList.count > 0 ? "CHOOSE" : "EDIT"
        __animated = true
    }
}
