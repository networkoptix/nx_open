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

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: parent.width / 6
        }

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

        QnIconButton {
            id: checkButton
            icon: "/images/check.png"
            x: CommonFunctions.dp(16)
            anchors.verticalCenter: parent.verticalCenter

            onClicked: {
                LoginDialogFunctions.clearSelection()
                actionBar.state = "HIDDEN"
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

                visible: savedSessionsList.selection.length || __currentIndex < savedSessionsList.count

                onClicked: {
                    if (loginDialog.state == "CHOOSE") {
                        LoginDialogFunctions.deleteSelected()
                    } else {
                        settings.removeSession(__currentIndex)
                        savedSessionsList.model = settings.savedSessions()
                        loginDialog.state = "CHOOSE"
                    }
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
                PropertyChanges {
                    target: deleteButton
                    __clipped: true
                }
                PropertyChanges {
                    target: backButton
                    __clipped: true
                }
                PropertyChanges {
                    target: checkButton
                    __clipped: true
                }
            },
            State {
                name: "SELECT"
                PropertyChanges {
                    target: actionBar
                    y: 0
                }
                PropertyChanges {
                    target: deleteButton
                    __clipped: false
                }
                PropertyChanges {
                    target: backButton
                    __clipped: true
                    visible: false
                }
                PropertyChanges {
                    target: checkButton
                    __clipped: false
                    visible: true
                }
            },
            State {
                name: "EDIT"
                PropertyChanges {
                    target: actionBar
                    y: 0
                }
                PropertyChanges {
                    target: deleteButton
                    __clipped: false
                }
                PropertyChanges {
                    target: backButton
                    __clipped: false
                    visible: true
                }
                PropertyChanges {
                    target: checkButton
                    __clipped: true
                    visible: false
                }
            }
        ]

        transitions: [
            Transition {
                from: "HIDDEN"
                to: "SELECT"
                SequentialAnimation {
                    PropertyAction { target: checkButton; property: "__clipped" }
                    PauseAnimation { duration: 50 }
                    PropertyAction { target: deleteButton; property: "__clipped" }
                }
            },
            Transition {
                from: "HIDDEN"
                to: "EDIT"
                SequentialAnimation {
                    PauseAnimation { duration: 150 }
                    PropertyAction { target: backButton; property: "__clipped" }
                    PauseAnimation { duration: 50 }
                    PropertyAction { target: deleteButton; property: "__clipped" }
                }
            }
        ]
    }

    Item {
        id: savedSessions

        anchors {
            top: newSession.top
            bottom: newSession.bottom
            topMargin: parent.width / 6
        }
        width: parent.width

        Text {
            id: savedSessionsLabel
            anchors.horizontalCenter: parent.horizontalCenter
            color: __syspal.windowText
            text: qsTr("Saved sessions")
            renderType: Text.NativeRendering
            font.pixelSize: CommonFunctions.dp(40)
        }

        ListView {
            id: savedSessionsList

            property var selection: []

            anchors {
                top: savedSessionsLabel.bottom
                topMargin: CommonFunctions.dp(8)
                bottom: parent.bottom
                horizontalCenter: parent.horizontalCenter
            }
            width: parent.width * 2 / 3
            spacing: CommonFunctions.dp(8)
            boundsBehavior: ListView.StopAtBounds
            clip: true

            delegate: Item {
                id: sessionItem

                width: savedSessionsList.width
                height: label.height + CommonFunctions.dp(16)

                Rectangle {
                    anchors.fill: parent

                    color: savedSessionsList.selection.indexOf(index) != -1 ? "#4a4a4a" : "#2d2d2d"
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
                        if (savedSessionsList.selection.length) {
                            LoginDialogFunctions.select(index)
                        } else {
                            __currentIndex = index
                            LoginDialogFunctions.updateUi(modelData)
                            loginDialog.state = "EDIT"
                        }
                    }
                    onPressAndHold: LoginDialogFunctions.select(index)
                }
            }
        }
    }

    Item {
        id: newSession

        anchors {
            top: logo.bottom
            bottom: fab.top
            bottomMargin: CommonFunctions.dp(32)
            left: savedSessions.right
        }
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
                inputMethodHints: Qt.ImhDigitsOnly

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
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
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

    Item {
        id: fab

        width: addButton.width
        height: addButton.height

        anchors {
            bottom: parent.bottom
            bottomMargin: parent.width / 6
            rightMargin: parent.width / 6
            horizontalCenter: loginDialog.horizontalCenter
        }

        QnRoundButton {
            id: addButton

            color: "#0096ff"
            icon: "/images/plus.png"

            onClicked: {
                LoginDialogFunctions.updateUi(null)
                __currentIndex = savedSessionsList.count
                LoginDialogFunctions.clearSelection()
                loginDialog.state = "EDIT"
            }
        }

        QnRoundButton {
            id: loginButton

            color: "#0096ff"
            icon: "/images/forward.png"
            opacity: visible ? 1.0 : 0.0
            visible: loginDialog.state == "EDIT"

            Behavior on opacity { NumberAnimation { duration: 200 } }

            onClicked: LoginDialogFunctions.connectToServer()
        }
    }

    states: [
        State {
            name: "CHOOSE"
            PropertyChanges {
                target: savedSessions
                x: 0
            }
            AnchorChanges {
                target: fab
                anchors.horizontalCenter: loginDialog.horizontalCenter
                anchors.right: undefined
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
            AnchorChanges {
                target: fab
                anchors.horizontalCenter: undefined
                anchors.right: loginDialog.right
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
            AnchorAnimation {
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
            AnchorAnimation {
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
