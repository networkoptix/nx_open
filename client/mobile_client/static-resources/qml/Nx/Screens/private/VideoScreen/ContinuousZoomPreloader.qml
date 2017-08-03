import QtQuick 2.6
import QtQuick.Window 2.0

BasePreloader
{
    id: control

    property color color: Qt.rgba(1, 1, 1, 0.8)
    property real thickness: 4
    property int period: 1500
    property int figuresCount: 4
    property bool zoomInIndicator: true

    implicitWidth: 96
    implicitHeight: 96

    Repeater
    {
        id: repeater

        model: figuresCount

        Rectangle
        {
            id: figure

            width: radius * 2
            height: radius * 2
            anchors.centerIn: parent
            color: "transparent"
            radius: parent.width / 2
            opacity: 0
            border.color: control.color
            border.width: control.thickness

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
                            duration: positionAnimation.duration / (control.zoomInIndicator ? 2 : 4)
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
                            duration: positionAnimation.duration / (control.zoomInIndicator ? 4 : 2)
                            easing.type: Easing.OutQuad

                            from: 1
                            to: 0
                        }
                    }

                    NumberAnimation
                    {
                        id: positionAnimation

                        target: figure
                        property: "radius"
                        duration: control.period
                        easing.type: Easing.Linear

                        from: control.zoomInIndicator ? control.width / 2 - control.thickness : 0
                        to: control.zoomInIndicator ? 0 : control.width / 2 - control.thickness
                    }
                }
            }
        }
    }
}
