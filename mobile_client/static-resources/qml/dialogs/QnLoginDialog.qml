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

    QnLoginSessionsModel {
        id: sessionsModel
    }


    /* backgorund */
    Rectangle {
        anchors.fill: parent
        color: __syspal.window
    }

    Rectangle {
        id: titleBar

        width: parent.width
        height: CommonFunctions.dp(56)

        color: colorTheme.color("nx_base")

        QnSideShadow {
            anchors.fill: titleBar
            position: Qt.BottomEdge
        }

        QnIconButton {
            id: backButton

            x: CommonFunctions.dp(4)
            anchors.verticalCenter: parent.verticalCenter
            size: CommonFunctions.dp(48)

            icon: "/images/back.png"

            onClicked: {
                loginDialog.state = "CHOOSE"
            }
        }

        QnIconButton {
            id: checkButton

            x: backButton.x
            anchors.verticalCenter: parent.verticalCenter
            size: CommonFunctions.dp(48)

            icon: "/images/check.png"

            onClicked: {
                LoginDialogFunctions.clearSelection()
                titleBar.state = "HIDDEN"
            }
        }

        QnIconButton {
            id: deleteButton

            anchors.right: parent.right
            anchors.rightMargin: CommonFunctions.dp(4)
            anchors.verticalCenter: parent.verticalCenter
            size: CommonFunctions.dp(48)

            icon: "/images/delete.png"

//            visible: savedSessionsList.selection.length || __currentIndex < savedSessionsList.count

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

        states: [
            State {
                name: "HIDDEN"
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
            top: titleBar.bottom
            bottom: parent.bottom
        }
        width: parent.width

        ListView {
            id: savedSessionsList

//            property var selection: []

            model: sessionsModel

            anchors.fill: parent
            boundsBehavior: ListView.StopAtBounds
            clip: true

            onCountChanged: console.log(count)

            delegate: Item {
                id: sessionItem

                width: savedSessionsList.width
                height: CommonFunctions.dp(72)

                Rectangle {
                    anchors.fill: parent

//                    color: savedSessionsList.selection.indexOf(index) != -1 ? "#4a4a4a" : "#2d2d2d"
                    color: "white"
                }

                Text {
                    id: label
                    x: CommonFunctions.dp(8)
                    anchors.verticalCenter: parent.verticalCenter
                    text: systemName
                    renderType: Text.NativeRendering
                    color: __syspal.windowText
                }

//                MouseArea {
//                    anchors.fill: parent
//                    onClicked: {
//                        if (savedSessionsList.selection.length) {
//                            LoginDialogFunctions.select(index)
//                        } else {
//                            __currentIndex = index
//                            LoginDialogFunctions.updateUi(modelData)
//                            loginDialog.state = "EDIT"
//                        }
//                    }
//                    onPressAndHold: LoginDialogFunctions.select(index)
//                }
            }
        }
    }

    Item {
        id: newSession

        anchors {
            top: titleBar.bottom
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

    QnRoundShadow {
        anchors.fill: fab
    }
    Item {
        id: fab

        width: addButton.width
        height: addButton.height

        anchors {
            bottom: parent.bottom
            right: parent.right
            margins: CommonFunctions.dp(24)
        }

        QnRoundButton {
            id: addButton

            color: colorTheme.color("nx_base")
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

            color: colorTheme.color("nx_base")
            icon: "/images/forward.png"
            opacity: visible ? 1.0 : 0.0
            visible: loginDialog.state == "EDIT"

            Behavior on opacity { NumberAnimation { duration: 200 } }

            onClicked: LoginDialogFunctions.connectToServer()
        }

        Behavior on rotation {
            RotationAnimation {
                easing.type: Easing.OutQuad
                duration: 200
                direction: RotationAnimation.Counterclockwise
            }
        }
    }

    states: [
        State {
            name: "CHOOSE"
            PropertyChanges {
                target: savedSessions
                x: 0
            }
            PropertyChanges {
                target: titleBar
                state: "HIDDEN"
            }
            PropertyChanges {
                target: fab
                rotation: -180.0
            }
        },
        State {
            name: "EDIT"
            PropertyChanges {
                target: savedSessions
                x: -loginDialog.width
            }
            PropertyChanges {
                target: titleBar
                state: "EDIT"
            }
            PropertyChanges {
                target: fab
                rotation: 0.0
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
        state = savedSessionsList.count > 0 ? "CHOOSE" : "EDIT"
        __animated = true
    }
}
