import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

Window {
    title: "test"
    visible: true

    Rectangle {
        anchors.fill: parent
        color: "#ffffff"
        Text {
            id: label
            text: "Hello HD"
            color: "#000000"
            anchors.centerIn: parent
        }
        MouseArea {
            anchors.fill: parent
            onClicked: Qt.quit()
        }
        Button {
            anchors.bottom: parent.bottom
            text: qsTr("Connect")
            onClicked: context.connectionManager.connect("http://admin:123@127.0.0.1:7001")
        }
    }

    context.connectionManager.onConnected: label.text = "Connected!!!"
}
