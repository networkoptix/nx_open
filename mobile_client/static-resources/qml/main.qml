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
        Column {
            anchors.bottom: parent.bottom

            TextField {
                id: urlField
                text: "http://admin:123@10.0.2.2:7001"
            }

            Button {
                text: qsTr("Connect")
                onClicked: connectionManager.connectToServer(urlField.text)
            }
        }
    }

    Connections {
        target: connectionManager
        onConnected: label.text = "Connected!!!"
        onConnectionFailed: label.text = "Failed! Reason = " + status
    }
}
