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

    Flickable {
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
                height: Math.max(0, panel.height - savedSessions.height - bottomButtons.height)
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
                    height: dp(16)
                }

                QnSideNavigationButtonItem {
                    id: newConnectionButton
                    text: qsTr("New connection")
                    icon: "qrc:///images/plus.png"
                    active: !activeSessionId

                    onClicked: Main.gotoNewSession()
                }

                Text {
                    x: dp(16)
                    height: dp(48)
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("© 2015 Network Optix")
                    font.pixelSize: sp(12)
                    color: QnTheme.sideNavigationCopyright
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
