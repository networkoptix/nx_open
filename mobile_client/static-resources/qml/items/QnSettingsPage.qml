import QtQuick 2.4
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2

import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"

QnPage {
    id: settingsPage

    title: qsTr("Settings")

    Connections {
        target: menuBackButton
        onClicked: Main.gotoMainScreen()
    }

    QnFlickable {
        id: flickable

        contentWidth: width
        contentHeight: content.height
        leftMargin: dp(16)
        rightMargin: dp(16)
        bottomMargin: dp(16)

        Column {
            id: content
            width: parent.width - parent.leftMargin - parent.rightMargin

            CheckBox {
                text: qsTr("Show offline cameras")
                checked: settings.showOfflineCameras
                onCheckedChanged: settings.showOfflineCameras = checked
                style: CheckBoxStyle {
                    label: Item {
                        Text {
                            text: control.text
                            color: QnTheme.windowText
                            anchors.verticalCenter: parent.verticalCenter
                            x: dp(24)
                        }
                    }
                }
            }
        }
    }
}

