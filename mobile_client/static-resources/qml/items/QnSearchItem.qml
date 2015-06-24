import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    height: toolBar.height

    width: searchPanel.visible ? searchPanel.width : searchButton.width

    Connections {
        target: menuBackButton
        onClicked: {
            if (!menuBackButton.menuOpened)
                return

            searchPanel.opacity = 0.0
            menuBackButton.animateToMenu()
            sideNavigation.enabled = true
            searchField.text = ""
        }
    }

    QnIconButton {
        id: searchButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        explicitRadius: dp(18)
        icon: "qrc:///images/search.png"

        onClicked: {
            searchPanel.opacity = 1.0
            menuBackButton.animateToBack()
            sideNavigation.enabled = false
            searchField.forceActiveFocus()
        }
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

        }

        QnIconButton {
            id: clearButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            explicitRadius: dp(18)
            icon: "qrc:///images/clear.png"

            onClicked: searchField.text = ""
        }
    }
}
