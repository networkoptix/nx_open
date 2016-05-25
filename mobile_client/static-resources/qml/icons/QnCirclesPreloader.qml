import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: preloader

    width: dp(60)
    height: dp(60)

    Repeater {
        model: 3
        delegate: circle
    }

    Component {
        id: circle

        Rectangle {
            id: circleRectangle
            anchors.centerIn: parent
            radius: width / 2
            border.width: dp(2)
            border.color: QnTheme.preloaderCircle
            color: "transparent"

            width: dp((index + 1) * 20) - border.width / 2
            height: width

            SequentialAnimation {
                id: animation
                running: true

                PauseAnimation {
                    duration: index * 200
                }

                SequentialAnimation {
                    loops: Animation.Infinite

                    NumberAnimation {
                        duration: 500
                        target: circleRectangle
                        property: "opacity"
                        from: 0
                        to: 1
                    }
                    NumberAnimation {
                        duration: 500
                        target: circleRectangle
                        property: "opacity"
                        from: 1
                        to: 0
                    }
                    PauseAnimation {
                        duration: 300
                    }
                }
            }
        }
    }
}

