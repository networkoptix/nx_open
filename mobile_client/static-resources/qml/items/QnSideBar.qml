import QtQuick 2.2
import Material 0.1
import Material.ListItems 0.1 as ListItem

Item {
    id: rootItem

    Column {
        id: topPart

        width: parent.width
        height: parent.height - bottomPart.height

        Item {
            id: titleItem
        }

        ListItem.Standard {
            text: qsTr("Servers")
        }

        ListItem.Standard {
            text: qsTr("Cameras")
        }

        ThinDivider {}

        ListItem.Header {
            text: qsTr("Recent cameras")
        }
    }

    Column {
        id: bottomPart
        width: parent.width
        anchors.bottom: parent.bottom

        ThinDivider {}

        ListItem.Standard {
            text: qsTr("Logout")

            onTriggered: connectionManager.disconnectFromServer()
        }
    }
}
