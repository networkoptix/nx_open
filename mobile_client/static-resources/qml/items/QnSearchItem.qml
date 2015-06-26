import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: searchItem

    property alias text: searchField.text
    readonly property bool opened: searchPanel.opacity == 1.0

    height: toolBar.height
    width: searchPanel.visible ? searchPanel.width : searchButton.width
    anchors.right: parent.right
    anchors.rightMargin: dp(8)

    QnIconButton {
        id: searchButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        explicitRadius: dp(18)
        icon: "qrc:///images/search.png"

        onClicked: open()
    }

    Rectangle {
        id: searchPanel
        color: QnTheme.windowBackground
        height: parent.height
        width: toolBar.width - dp(72)

        opacity: 0.0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 150 } }

        QnTextField {
            id: searchField

            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                right: parent.right
                rightMargin: dp(8)
            }

            placeholderText: qsTr("Search")

            rightPadding: dp(48)
            showDecoration: false
        }

        QnIconButton {
            id: clearButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            explicitRadius: dp(18)
            icon: "qrc:///images/clear.png"

            onClicked: clear()
        }
    }

    function open() {
        searchPanel.opacity = 1.0
        searchField.forceActiveFocus()
    }

    function close() {
        searchPanel.opacity = 0.0
        clear()
    }

    function clear() {
        searchField.text = ""
    }
}
