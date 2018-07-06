import QtQuick 2.0;
import Qt.labs.controls 1.0;
import Nx 1.0;

BusyIndicator
{
    id: control;

    property color color: ColorTheme.mid;

    implicitWidth: (repeater.count > 0 ? radiusByIndex(repeater.count - 1) * 2 : 30);
    implicitHeight: implicitWidth;
    contentItem: null;

    Repeater
    {
        id: repeater;

        model: 3;
        delegate: NxCircle
        {
            id: circleRectangle;

            anchors.centerIn: parent;
            radius: control.radiusByIndex(index)
            border.width: 2;
            border.color: control.color;
            color: "transparent";

            SequentialAnimation
            {
                running: control.running;

                PauseAnimation
                {
                    duration: index * 200;
                }

                SequentialAnimation
                {
                    loops: Animation.Infinite;

                    NumberAnimation
                    {
                        duration: 500;
                        target: circleRectangle;
                        property: "opacity";
                        from: 0;
                        to: 1;
                    }

                    NumberAnimation
                    {
                        duration: 500;
                        target: circleRectangle;
                        property: "opacity";
                        from: 1;
                        to: 0;
                    }

                    PauseAnimation
                    {
                        duration: 300;
                    }
                }
            }
        }
    }

    function radiusByIndex(index)
    {
        return (20 + index * 16);
    }
}

