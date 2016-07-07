import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0

BusyIndicator
{
    id: control

    property color color: ColorTheme.base13

    implicitWidth: 60
    implicitHeight: 60

    contentItem: null

    Repeater
    {
        model: 3
        delegate: circle
    }

    Component
    {
        id: circle

        Rectangle
        {
            id: circleRectangle

            anchors.centerIn: parent
            radius: width / 2
            border.width: 2
            border.color: control.color
            color: "transparent"

            width: (index + 1) * 20 - border.width / 2
            height: width

            SequentialAnimation
            {
                running: control.running

                PauseAnimation
                {
                    duration: index * 200
                }

                SequentialAnimation
                {
                    loops: Animation.Infinite

                    NumberAnimation
                    {
                        duration: 500
                        target: circleRectangle
                        property: "opacity"
                        from: 0
                        to: 1
                    }
                    NumberAnimation
                    {
                        duration: 500
                        target: circleRectangle
                        property: "opacity"
                        from: 1
                        to: 0
                    }
                    PauseAnimation
                    {
                        duration: 300
                    }
                }
            }
        }
    }
}

