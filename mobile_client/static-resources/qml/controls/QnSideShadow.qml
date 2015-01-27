import QtQuick 2.2

import "../common_functions.js" as CommonFunctions

Item {
    property int position

    readonly property int __orientation:
        position == Qt.TopEdge || position == Qt.BottomEdge ? Qt.Horizontal : Qt.Vertical

    readonly property real shadowSize: CommonFunctions.dp(8)

    clip: false
    z: -1.0

    Rectangle {
        width: __orientation == Qt.Horizontal ? parent.width : shadowSize
        height: __orientation == Qt.Vertical ? parent.height : shadowSize

        anchors {
            top: position == Qt.BottomEdge ? parent.bottom : undefined
            bottom: position == Qt.TopEdge ? parent.top : undefined
            left: position == Qt.RightEdge ? parent.right : undefined
            right: position == Qt.LeftEdge ? parent.left : undefined
        }

        rotation: position == Qt.TopEdge || position == Qt.LeftEdge ? 180 : 0

        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.15) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0) }
        }
    }
}
