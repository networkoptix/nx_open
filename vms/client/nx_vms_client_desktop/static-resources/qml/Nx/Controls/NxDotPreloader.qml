import QtQuick 2.6
import QtQuick.Controls 2.4

BusyIndicator
{
    id: control;

    property int dotsCount: 3;
    property int dotAnimationOffsetMs: 200;
    property color color: "white";

    contentItem: Row
    {
        spacing: 6;
        visible: control.running

        Repeater
        {
            model: control.dotsCount;

            delegate: NxCircle
            {
                id: dot;

                radius: 4;
                color: control.color;
                opacity: 0;

                SequentialAnimation on opacity
                {
                    id: opacityAnimation;
                    running: control.running

                    PauseAnimation
                    {
                        duration: index * control.dotAnimationOffsetMs;
                    }

                    SequentialAnimation
                    {
                        loops: Animation.Infinite;

                        PropertyAnimation
                        {
                            duration: 500;
                            easing.type: Easing.Linear;
                            to: 1;
                        }
                        PropertyAnimation
                        {
                            duration: 500;
                            easing.type: Easing.Linear;
                            to: 0;
                        }

                        PauseAnimation
                        {
                            duration: control.dotAnimationOffsetMs * (control.dotsCount - 1)
                        }
                    }
                }
            }
        }
    }
}
