import QtQuick 2.2
import Material 0.1

GridView {
    id: gridView

    cellWidth: width / 2
    cellHeight: cellWidth / 4 * 3

    delegate: Item {
        id: wrapper

        width: gridView.cellWidth
        height: gridView.cellHeight

        Image {
            anchors.fill: parent
            source: thumbnail
            fillMode: Qt.KeepAspectRatio
        }

        Text {
            anchors.bottom: parent.bottom
            text: resourceName
        }
    }
}
