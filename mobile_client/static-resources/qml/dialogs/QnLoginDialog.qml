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

    property string __currentId

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

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
                __currentId = ""
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

            onClicked: {
                if (loginDialog.state == "CHOOSE") {
                    LoginDialogFunctions.deleteSelected()
                } else {
                    sessionsModel.deleteSession(__currentId)
                    loginDialog.state = "CHOOSE"
                }
            }
        }

        Text {
            id: systemNameLabel
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            font.pixelSize: CommonFunctions.sp(20)
            font.weight: Font.DemiBold
            x: CommonFunctions.dp(72)
        }

        Text {
            id: defaultLabel
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            font.pixelSize: CommonFunctions.sp(20)
            font.weight: Font.DemiBold
            x: CommonFunctions.dp(72)
            text: applicationInfo.productName
        }

        states: [
            State {
                name: "HIDDEN"
                PropertyChanges {
                    target: deleteButton
                    __clipped: true
                    visible: false
                }
                PropertyChanges {
                    target: backButton
                    __clipped: true
                }
                PropertyChanges {
                    target: checkButton
                    __clipped: true
                }
                PropertyChanges {
                    target: systemNameLabel
                    opacity: 0.0
                }
                PropertyChanges {
                    target: defaultLabel
                    opacity: 1.0
                }
            },
            State {
                name: "SELECT"
                PropertyChanges {
                    target: deleteButton
                    __clipped: false
                    visible: true
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
                PropertyChanges {
                    target: systemNameLabel
                    opacity: 0.0
                }
                PropertyChanges {
                    target: defaultLabel
                    opacity: 1.0
                }
            },
            State {
                name: "EDIT"
                PropertyChanges {
                    target: deleteButton
                    __clipped: false
                    visible: __currentId
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
                PropertyChanges {
                    target: systemNameLabel
                    opacity: 1.0
                }
                PropertyChanges {
                    target: defaultLabel
                    opacity: 0.0
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
                    ParallelAnimation {
                        SequentialAnimation {
                            PauseAnimation { duration: 50 }
                            NumberAnimation { target: defaultLabel; property: "opacity"; duration: 100; easing.type: Easing.OutQuad }
                        }
                        PauseAnimation { duration: 150 }
                    }
                    NumberAnimation { target: systemNameLabel; property: "opacity"; duration: 100; easing.type: Easing.OutQuad }
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

            property var selection: []

            model: sessionsModel

            anchors.fill: parent
            boundsBehavior: ListView.StopAtBounds
            clip: true

            onCountChanged: console.log(count)

            section.property: "section"
            section.criteria: ViewSection.FullString
            section.labelPositioning: ViewSection.CurrentLabelAtStart | ViewSection.InlineLabels

            section.delegate: Item {
                height: CommonFunctions.dp(48)
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: CommonFunctions.sp(16)
                    renderType: Text.NativeRendering
                    opacity: 0.7
                    x: CommonFunctions.dp(16)

                    text: section
                }
            }

            delegate: Item {
                id: sessionItem

                property bool selected: LoginDialogFunctions.isSelected(sessionId)

                width: contentItem.width
                height: CommonFunctions.dp(72)

                Rectangle {
                    id: highlight
                    anchors.fill: parent

                    color: Qt.rgba(0, 0, 0, 0.12)
                    visible: selected
                }

                Text {
                    id: body1
                    x: CommonFunctions.dp(72)
                    y: CommonFunctions.dp(36) - height
                    font.pixelSize: CommonFunctions.sp(14)
                    font.weight: Font.DemiBold
                    renderType: Text.NativeRendering
                    opacity: 0.87

                    text: systemName ? systemName : qsTr("<UNKNOWN>")
                }

                Text {
                    id: caption
                    x: CommonFunctions.dp(72)
                    y: body1.y + CommonFunctions.sp(20)
                    font.pixelSize: CommonFunctions.sp(12)
                    renderType: Text.NativeRendering
                    opacity: 0.7

                    text: {
                        if (user)
                            return user + '@' + address + ':' + port
                        else
                            address + ':' + port
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (savedSessionsList.selection.length) {
                            LoginDialogFunctions.select(sessionId)
                        } else {
                            if (user) {
                                LoginDialogFunctions.connectToServer(address, port, user, password)
                            } else {
                                LoginDialogFunctions.editSession(systemName, address, port, user, password)
                                loginDialog.state = "EDIT"
                            }
                        }
                    }
                    onPressAndHold: {
                        if (user)
                            LoginDialogFunctions.select(sessionId)
                    }
                }

                QnIconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: CommonFunctions.dp(16)
                    icon: "/images/icons/edit_black.png"
                    opacity: 0.7
                    size: CommonFunctions.dp(48)

                    visible: user

                    onClicked: {
                        LoginDialogFunctions.editSession(systemName, address, port, user, password)
                        __currentId = sessionId
                        loginDialog.state = "EDIT"
                    }
                }
            }
        }
    }

    Item {
        id: sessionEditor

        anchors {
            top: titleBar.bottom
            bottom: parent.bottom
            left: savedSessions.right
        }
        width: parent.width

        Column {
            id: fieldsColumn

            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: CommonFunctions.dp(16)
            spacing: CommonFunctions.dp(8)

            Text {
                text: qsTr("Address")
                font.pixelSize: CommonFunctions.sp(12)
                opacity: 0.7
            }

            QnTextField {
                id: address
                placeholderText: "127.0.0.1"
                width: parent.width
                onAccepted: port.focus = true
            }

            Text {
                text: qsTr("Port")
                font.pixelSize: CommonFunctions.sp(12)
                opacity: 0.7
            }

            QnTextField {
                id: port
                width: parent.width
                inputMethodHints: Qt.ImhDigitsOnly

                validator: IntValidator {
                    bottom: 1
                    top: 65535
                }
                text: "7001"

                onAccepted: login.focus = true
            }

            Text {
                text: qsTr("User name")
                font.pixelSize: CommonFunctions.sp(12)
                opacity: 0.7
            }

            QnTextField {
                id: login
                placeholderText: "admin"
                width: parent.width
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onAccepted: password.focus = true
            }

            Text {
                text: qsTr("Password")
                font.pixelSize: CommonFunctions.sp(12)
                opacity: 0.7
            }

            QnTextField {
                id: password
                placeholderText: "*******"
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
                LoginDialogFunctions.clearSelection()
                LoginDialogFunctions.newSession()
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

            onClicked: LoginDialogFunctions.connectToServer(address.text, port.text, login.text, password.text)
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
    state: "CHOOSE"

    transitions: Transition {
        NumberAnimation {
            target: savedSessions
            property: "x"
            easing.type: Easing.OutQuad
            duration: 200
        }
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            if (loginDialog.state == "CHOOSE")
                Qt.quit()
            else
                loginDialog.state = "CHOOSE"
        }
    }
}
