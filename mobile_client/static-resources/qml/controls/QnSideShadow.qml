import QtQuick 2.4

Item {
    property int position

    readonly property int _orientation:
        position == Qt.TopEdge || position == Qt.BottomEdge ? Qt.Horizontal : Qt.Vertical

    readonly property real shadowSize: dp(8)

    z: -1.0

    Rectangle {
        width: _orientation == Qt.Horizontal ? parent.width : parent.height
        height: shadowSize

        anchors {
            top: position == Qt.BottomEdge ? parent.bottom : undefined
            bottom: position == Qt.TopEdge ? parent.top : undefined
            left: position == Qt.RightEdge ? parent.right : undefined
            right: position == Qt.LeftEdge ? parent.left : undefined
        }

        rotation: {
            if (position == Qt.TopEdge)
                return 180
            else if (position == Qt.LeftEdge)
                return -90
            else if (position == Qt.RightEdge)
                return 90
            else
                return 0
        }

        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.15) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0) }
        }
    }
}
