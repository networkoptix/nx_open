import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

BusyIndicator
{
    id: control

    property color color: ColorTheme.base9

    property int dotSpacing: 8
    property int dotRadius: 12

    contentItem: Row
    {
        id: content

        anchors.centerIn: parent
        spacing: control.dotSpacing
        opacity: control.running ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Repeater
        {
            model: 3
            delegate: dot
        }
    }

    Component
    {
        id: dot

        Rectangle
        {
            id: dotRectangle
            width: control.dotRadius
            height: width
            radius: width / 2
            color: control.color
            opacity: 0.0

            SequentialAnimation
            {
                id: animation
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
                        target: dotRectangle
                        property: "opacity"
                        from: 0
                        to: 1
                    }
                    NumberAnimation
                    {
                        duration: 500
                        target: dotRectangle
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

