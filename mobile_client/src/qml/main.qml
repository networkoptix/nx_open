import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.2

Window {
    id: screen
    visible: true
    width: 360
    height: 360

    Grid {
        id: grid1
        anchors.fill: parent
        antialiasing: false
        scale: 1
        spacing: 0
        layoutDirection: Qt.LeftToRight
        rows: 5
        columns: 2

        Label {
            id: label1
            width: 47
            height: 28
            text: qsTr("Host")
        }

        TextField {
            id: textField1
            placeholderText: qsTr("Text Field")
        }

        Label {
            id: label2
            text: qsTr("Port")
        }

        SpinBox {
            id: spinBox1
        }

        Label {
            id: label3
            text: qsTr("Login")
        }

        TextField {
            id: textField2
            placeholderText: qsTr("Text Field")
        }

        Label {
            id: label4
            text: qsTr("Password")
        }

        TextField {
            id: textField3
            placeholderText: qsTr("Text Field")
        }

        Label {
            id: label5
            text: qsTr(" ")
        }

        Button {
            id: button1
            text: qsTr("Connect")

            onClicked: {
                screen.color = "#ff0000"
            }
        }
    }
}











