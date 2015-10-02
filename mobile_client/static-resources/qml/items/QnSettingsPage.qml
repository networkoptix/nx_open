import QtQuick 2.4

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

        anchors.fill: parent

        contentWidth: width
        contentHeight: content.height
        leftMargin: dp(16)
        rightMargin: dp(16)
        bottomMargin: dp(16)

        Column {
            id: content
            width: flickable.width - flickable.leftMargin - flickable.rightMargin

            QnCheckBox {
                text: qsTr("Show offline cameras")
                checked: settings.showOfflineCameras
                onCheckedChanged: settings.showOfflineCameras = checked
            }
        }
    }

    focus: true

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            if (Main.backPressed())
                event.accepted = true
        }
    }
}

