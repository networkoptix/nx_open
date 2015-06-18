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

                    Rectangle {
                        width: parent.width
                        height: dp(120)
                        color: active ? QnTheme.sessionItemBackgroundActive : QnTheme.sideNavigationBackground

                        property bool active: activeSessionId == sessionId

                        MouseArea {
                            anchors.fill: parent
                            onClicked: LoginFunctions.connectToServer(address, port, user, password)
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: dp(8)
                            x: dp(16)
                            width: parent.width - 2 * x

                            Text {
                                id: systemNameLabel
                                text: systemName
                                font.pixelSize: dp(20)
                                font.weight: Font.DemiBold
                                color: QnTheme.listText
                                elide: Text.ElideRight
                                width: parent.width - dp(16) - editButton.width

                                Rectangle {
                                    id: editButton
                                    color: QnTheme.listSubText
                                    width: dp(20)
                                    height: dp(20)
                                    anchors {
                                        verticalCenter: parent.verticalCenter
                                        left: parent.right
                                        margins: dp(16)
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: Main.openSavedSession(sessionId, address, port, user, password, systemName)
                                    }
                                }
                            }

                            Text {
                                text: address
                                font.pixelSize: dp(16)
                                font.weight: Font.DemiBold
                                color: QnTheme.listSubText
                            }

                            Text {
                                text: user
                                font.pixelSize: dp(16)
                                font.weight: Font.DemiBold
                                color: QnTheme.listSubText
                            }
                        }
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
