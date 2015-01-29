import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.2

import Material 0.1
import Material.ListItems 0.1 as ListItem

import com.networkoptix.qml 1.0

import "../controls"

import "login_dialog_functions.js" as LoginDialogFunctions
import "../common_functions.js" as CommonFunctions

Page {
    id: loginPage

    property string __currentId

    title: applicationInfo.productName

    actions: [
        Action {
            iconName: "action/done"
            name: qsTr("Deselect")
            visible: sessionsList.selection.length > 0
            onTriggered: sessionsList.selection = []
        },
        Action {
            iconName: "action/delete"
            name: qsTr("Remove")
            visible: sessionsList.selection.length > 0
            onTriggered: {
                if (__currentId != "")
                    sessionsModel.deleteSession(__currentId)
                else
                    LoginDialogFunctions.deleteSesions(sessionsList.selection)
            }
        }
    ]

    QnLoginSessionsModel {
        id: sessionsModel
    }

    ListView {
        id: sessionsList

        property var selection: []

        model: sessionsModel

        anchors.fill: parent
        boundsBehavior: ListView.StopAtBounds
        clip: true

        section.property: "section"
        section.criteria: ViewSection.FullString
        section.labelPositioning: ViewSection.CurrentLabelAtStart | ViewSection.InlineLabels

        section.delegate: ListItem.Header {
            Rectangle {
                anchors.fill: parent
                color: theme.backgroundColor
                z: -1.0
            }

            text: section
        }

        delegate: ListItem.Subtitled {
            text: systemName ? systemName : qsTr("<UNKNOWN>")
            subText: {
                if (user)
                    return user + '@' + address + ':' + port
                else
                    address + ':' + port
            }

            secondaryItem: IconButton {
                action: Action {
                    iconName: "image/edit"
                    name: qsTr("Modify")
                    onTriggered: sessionEditDialog.openExistingSession(systemName, address, port, user, password)
                }
                visible: user
            }

            selected: sessionsList.selection.indexOf(sessionId) != -1

            onTriggered: {
                if (sessionsList.selection.length) {
                    if (user)
                        LoginDialogFunctions.select(sessionId)
                } else {
                    if (user) {
                        LoginDialogFunctions.connectToServer(address, port, user, password)
                    } else {
                        __currentId = sessionId
                        sessionEditDialog.openExistingSession(systemName, address, port, user, password)
                    }
                }
            }
            onPressAndHold: {
                if (user)
                    LoginDialogFunctions.select(sessionId)
            }
        }
    }

    ActionButton {
        anchors {
            bottom: parent.bottom
            right: parent.right
            margins: units.dp(16)
        }
        iconName: "content/add"
        onClicked: sessionEditDialog.openNewSession()
    }

    QnSessionEditDialog {
        id: sessionEditDialog
        visible: false
        onAccepted: LoginDialogFunctions.connectToServer(address, port, user, password)
        onVisibleChanged: {
            if (!visible)
                __currentId = ""
        }
    }

    Connections {
        id: connections
        target: connectionManager
        onConnected: {
            LoginDialogFunctions.saveCurrentSession()
        }
    }
}
