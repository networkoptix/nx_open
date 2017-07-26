import QtQuick 2.6
import QtQuick.Window 2.0

BasePreloader
{
    id: control

    property color color: Qt.rgba(1, 1, 1, 1)
    property real sideSize: height / 2.5
    property real thickness: 6
    property int period: 2200
    property int figuresCount: 5

    implicitWidth: implicitHeight * 1.5
    implicitHeight: 96

    Repeater
    {
        id: repeater

        model: figuresCount

        Item
        {
            id: figure

            property real xPosition: 0
            readonly property real figureWidth:
                (control.sideSize + control.thickness) * Math.cos(Math.PI / 4) / 2

            x: xPosition - figureWidth
            width: control.sideSize
            height: control.sideSize
            anchors.verticalCenter: parent.verticalCenter
            rotation: 45
            opacity: 0

            Rectangle
            {
                width: parent.width
                height: control.thickness

                color: control.color
            }

            Rectangle
            {
                y: control.thickness
                x: parent.width - width
                width: control.thickness
                height: control.sideSize - control.thickness

                color: control.color
            }

            SequentialAnimation
            {
                running: true

                PauseAnimation
                {
                    duration: index * control.period / control.figuresCount
                }

                ParallelAnimation
                {
                    loops: Animation.Infinite

                    SequentialAnimation
                    {
                        NumberAnimation
                        {
                            id: fadeInAnimation

                            target: figure
                            property: "opacity"
                            duration: positionAnimation.duration / 2
                            easing.type: Easing.InQuad

                            from: 0
                            to: 1
                        }

                        PauseAnimation
                        {
                            duration: positionAnimation.duration
                                - fadeInAnimation.duration - fadeOutAnimation.duration
                        }

                        NumberAnimation
                        {
                            id: fadeOutAnimation

                            target: figure
                            property: "opacity"
                            duration: positionAnimation.duration / 4
                            easing.type: Easing.OutQuad

                            from: 1
                            to: 0
                        }
                    }

                    NumberAnimation
                    {
                        id: positionAnimation

                        target: figure
                        property: "xPosition"
                        duration: control.period
                        easing.type: Easing.Linear

                        from: 0
                        to: control.width - figure.figureWidth *  2
                    }
                }
            }
        }
    }
}
