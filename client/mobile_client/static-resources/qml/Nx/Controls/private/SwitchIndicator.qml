import QtQuick 2.6
import Nx 1.0

Item
{
    property bool checked: false

    property color uncheckedColor: ColorTheme.base13
    property color uncheckedIndicatorColor: ColorTheme.base17
    property color checkedColor: ColorTheme.green_main
    property color checkedIndicatorColor: ColorTheme.green_l2
    property color handleColor: ColorTheme.base5

    property int animationDuration: 150

    implicitWidth: 38
    implicitHeight: 20

    layer.enabled: !enabled
    opacity: enabled ? 1.0 : 0.3

    Rectangle
    {
        id: uncheckedLayer

        anchors.fill: parent

        radius: height / 2
        color: uncheckedColor

        Rectangle
        {
            id: uncheckedindicator

            width: parent.height / 2 + 2
            height: width
            radius: height / 2
            x: parent.width - parent.height / 2 - width / 2
            anchors.verticalCenter: parent.verticalCenter
            color: uncheckedIndicatorColor

            Rectangle
            {
                width: parent.width - 4
                height: width
                radius: height / 2
                anchors.centerIn: parent
                color: uncheckedColor
            }
        }

        Rectangle
        {
            id: checkedLayer

            anchors.fill: parent
            radius: height / 2
            color: checkedColor

            Rectangle
            {
                id: checkedIndicator

                width: 2
                height: parent.height / 2
                x: parent.height / 2
                anchors.verticalCenter: parent.verticalCenter
                color: checkedIndicatorColor
            }

            opacity: checked ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: animationDuration } }
        }

        Rectangle
        {
            id: handle

            width: parent.height - 4
            height: width
            radius: height / 2
            color: handleColor
            anchors.verticalCenter: parent.verticalCenter

            x: checked ? parent.width - width - 2 : 2
            Behavior on x { NumberAnimation { duration: animationDuration } }
        }
    }
}
