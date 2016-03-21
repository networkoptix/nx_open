import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"
import "../main.js" as Main
import "../items/QnLoginPage.js" as LoginFunctions

QnNavigationDrawer {
    id: panel

    property string activeSessionId
    readonly property alias savedSessionsModel: savedSessionsModel

    color: QnTheme.sideNavigationBackground

    QnFlickable {
        id: flickable

        width: panel.width
        anchors.top: parent.top
        anchors.bottom: bottomButtons.top
        clip: true

        flickableDirection: Flickable.VerticalFlick
        contentHeight: content.height

        Column {
            id: content
            width: panel.width

            Column {
                id: savedSessions

                width: parent.width

                Item {
                    width: parent.width
                    height: getStatusBarHeight()
                }

                Text {
                    id: sectionLabel
                    height: dp(56)
                    x: dp(16)
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Saved connections")
                    font.pixelSize: sp(16)
                    font.weight: Font.DemiBold
                    color: QnTheme.sideNavigationSection

                    Rectangle {
                        width: content.width - 2 * sectionLabel.x
                        height: dp(1)
                        anchors.bottom: sectionLabel.bottom
                        anchors.bottomMargin: dp(12)
                        color: QnTheme.sideNavigationSection
                    }
                }

                Repeater {
                    id: savedSessionsList

                    width: parent.width

                    model: QnSavedSessionsModel {
                        id: savedSessionsModel
                    }

                    QnSessionItem {
                        sessionId: model.sessionId
                        systemName: model.systemName
                        address: model.address
                        port: model.port
                        user: model.user
                        password: model.password
                        active: activeSessionId == sessionId
                    }
                }
            }

            Item {
                id: spacer
                width: parent.width
                height: Math.max(dp(8), panel.height - savedSessions.height - bottomButtons.height)
            }
        }
    }

    Column {
        id: bottomButtons

        width: parent.width
        anchors.bottom: parent.bottom

        Rectangle {
            width: parent.width
            height: dp(9)
            color: panel.color

            Rectangle {
                x: dp(16)
                width: parent.width - 2 * x
                height: dp(1)
                color: QnTheme.sideNavigationSplitter
            }
        }

        QnSideNavigationButtonItem {
            id: newConnectionButton
            text: qsTr("New connection")
            icon: "image://icon/plus.png"
            active: !activeSessionId

            onClicked: {
                panel.hide()
                Main.gotoNewSession()
                stackView.currentItem.scrollTop()
            }
        }

        QnSideNavigationButtonItem {
            id: settingsButton
            text: qsTr("Settings")
            icon: "image://icon/settings.png"

            onClicked: Main.openSettings()
        }

        Item {
            width: parent.width
            height: navigationBarPlaceholder.height + dp(8)
            visible: navigationBarPlaceholder.height > 1
        }
    }
}
