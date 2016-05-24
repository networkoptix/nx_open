import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: preloader

    implicitWidth: content.width
    implicitHeight: content.height

    property color color: QnTheme.preloaderDot

    Row {
        id: content

        anchors.centerIn: parent

        spacing: dp(8)

        Repeater {
            model: 3
            delegate: dot
        }
    }

    Component {
        id: dot

        Rectangle {
            id: dotRectangle
            width: dp(12)
            height: width
            radius: width / 2
            color: preloader.color

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
                        target: dotRectangle
                        property: "opacity"
                        from: 0
                        to: 1
                    }
                    NumberAnimation {
                        duration: 500
                        target: dotRectangle
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

