import QtQuick.Controls 2.0
import QtQuick 2.5
import QtGraphicalEffects 1.0
import Nx 1.0

ToolTip
{
    id: control

    font.pixelSize: 13
    leftPadding: 8
    rightPadding: 8
    topPadding: 0
    bottomPadding: 0
    margins: 0

    delay: 500

    implicitHeight: 24

    background: Item
    {
        Rectangle
        {
            id: backgroundRect
            anchors.fill: parent
            color: ColorTheme.text
            radius: 2
            visible: false
        }

        DropShadow
        {
            anchors.fill: parent
            radius: 5
            verticalOffset: 3
            source: backgroundRect
            color: ColorTheme.transparent(ColorTheme.colors.dark4, 0.35)
        }
    }

    contentItem: Text
    {
        text: control.text
        font: control.font
        color: ColorTheme.colors.dark4
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    enter: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            to: 1
            duration: 150
        }
    }
    exit: Transition
    {
        SequentialAnimation
        {
            PauseAnimation { duration: 300 }
            NumberAnimation
            {
                property: "opacity"
                to: 0
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
    }
}
