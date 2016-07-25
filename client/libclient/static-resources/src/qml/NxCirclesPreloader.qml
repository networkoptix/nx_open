import QtQuick 2.0;
import Qt.labs.controls 1.0;

import "."

BusyIndicator
{
    id: control;

    property color color: Style.colors.mid;

    implicitWidth: 60;
    implicitHeight: implicitWidth;
    contentItem: null;

    Repeater
    {
        model: 3;
        delegate: NxCircle
        {
            id: circleRectangle;

            anchors.centerIn: parent;
            radius: 20 + index * 16;
            border.width: 2;
            border.color: control.color;
            color: "transparent";

            onWidthChanged:
            {
                if (width > control.implicitWidth)
                    control.implicitWidth = width;
            }

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
}

