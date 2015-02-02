import QtQuick 2.2
import Material 0.1

GridView {
    id: gridView

    property real spacing: units.dp(8)

    cellWidth: width / 2
    cellHeight: cellWidth / 4 * 3

    delegate: Item {
        id: wrapper

        x: spacing / 4
        y: spacing / 4

        width: gridView.cellWidth - spacing / 2
        height: gridView.cellHeight - spacing / 2

        Image {
            anchors.fill: parent
            source: thumbnail
            fillMode: Qt.KeepAspectRatio
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: label.height + label.anchors.margins * 2
            opacity: 0.7
        }

        Label {
            id: label
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: units.dp(8)
            text: resourceName
        }
    }

    Scrollbar { flickableItem: gridView }

    Timer {
        id: refreshTimer

        interval: 2 * 60 * 1000
        repeat: true
        running: true

        onTriggered: {
            model.refreshThumbnails(indexAt(0, contentY), indexAt(width - 1, contentY + height - 1))
        }
    }
}
