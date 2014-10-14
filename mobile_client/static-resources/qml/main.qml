import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "dialogs"
import "main.js" as Main

Window {
    title: "HD Witness"
    visible: true

    width: 480
    height: 800

    StackView {
        id: stackView

        anchors.fill: parent

        initialItem: QnLoginDialog {
            width: parent
            height: parent
        }
    }

    Connections {
        target: connectionManager
        onConnected: Main.openResources()
    }

    Component.onCompleted: Main.openLoginDialog()
}
