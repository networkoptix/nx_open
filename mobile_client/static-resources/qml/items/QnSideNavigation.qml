import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"
import "../main.js" as Main
import "../items/QnLoginPage.js" as LoginFunctions

QnNavigationDrawer {
    id: panel

    property string activeSessionId
    readonly property alias savedSessionsModel: savedSessionsModel

    parent: _findRootItem()

    color: QnTheme.sideNavigationBackground

    QnFlickable {
        id: flickable

        width: panel.width
        height: panel.height

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
                    height: dp(48)
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
                        anchors.bottomMargin: dp(8)
                        color: QnTheme.sideNavigationSection
                    }
                }

                Repeater {
                    id: savedSessionsList

                    width: parent.width

                    model: QnLoginSessionsModel {
                        id: savedSessionsModel
                        displayMode: QnLoginSessionsModel.ShowSaved
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

            Column {
                id: bottomButtons

                width: parent.width

                Rectangle {
                    x: dp(16)
                    width: parent.width - 2 * x
                    height: dp(1)
                    color: QnTheme.sideNavigationSplitter
                }

                Item {
                    width: parent.width
                    height: dp(8)
                }

                QnSideNavigationButtonItem {
                    id: newConnectionButton
                    text: qsTr("New connection")
                    icon: "image://icon/plus.png"
                    active: !activeSessionId

                    onClicked: {
                        panel.hide()
                        Main.gotoNewSession()
                    }
                }

                QnSideNavigationButtonItem {
                    id: settingsButton
                    text: qsTr("Settings")
                    icon: "image://icon/settings.png"

                    onClicked: Main.openSettings()
                }

                Text {
                    x: dp(16)
                    height: dp(48)
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("© 2015 Network Optix")
                    font.pixelSize: sp(12)
                    color: QnTheme.sideNavigationCopyright
                }

                Item {
                    width: parent.width
                    height: navigationBarPlaceholder.height + dp(8)
                    visible: navigationBarPlaceholder.height > 1
                }
            }
        }
    }

    function _findRootItem() {
        var p = panel;
        while (p.parent != null)
            p = p.parent;
        return p;
    }
}
