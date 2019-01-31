import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx 1.0

ScrollIndicator
{
    id: control

    implicitWidth: Math.max(indicator.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(indicator.implicitHeight + topPadding + bottomPadding)

    contentItem: Rectangle
    {
        id: indicator

        readonly property bool horizontal: control.orientation === Qt.Horizontal

        implicitWidth: 4
        implicitHeight: 4
        radius: horizontal ? height / 2 : width / 2

        color: ColorTheme.base9
        visible: control.size < 1.0
        opacity: 0.0

        x: control.leftPadding + (horizontal ? control.position * control.width : 0)
        y: control.topPadding + (horizontal ? 0 : control.position * control.height)
        width: horizontal ? control.size * control.availableWidth : implicitWidth
        height: horizontal ? implicitHeight : control.size * control.availableHeight

        states: State
        {
            name: "active"
            when: control.active
            PropertyChanges
            {
                target: indicator
                opacity: 1.0
            }
        }

        transitions:
        [
            Transition
            {
                from: "active"

                SequentialAnimation
                {
                    PauseAnimation { duration: 500 }
                    NumberAnimation
                    {
                        duration: 250
                        property: "opacity"
                        to: 0.0
                    }
                }
            }
        ]
    }
}
