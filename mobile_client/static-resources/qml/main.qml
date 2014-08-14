import QtQuick 2.2
import QtQuick.Window 2.0

Window {
    title: "test"
    visible: true

    Rectangle {
        anchors.fill: parent
        color: "#ffffff"
        Text {
            text: "Hello HD"
            color: "#000000"
            anchors.centerIn: parent
        }
        MouseArea {
            anchors.fill: parent
            onClicked: Qt.quit()
        }
    }
}
