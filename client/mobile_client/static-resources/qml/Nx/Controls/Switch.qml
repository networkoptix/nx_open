import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0

Control
{
    id: control

    property bool checked: false

    property color uncheckedColor: ColorTheme.base13
    property color uncheckedIndicatorColor: ColorTheme.base17
    property color checkedColor: ColorTheme.green_main
    property color checkedIndicatorColor: ColorTheme.green_l2
    property color handleColor: ColorTheme.base5

    property int animationDuration: 150

    signal clicked()

    padding: 6
    leftPadding: 8
    rightPadding: 8

    implicitWidth: 38 + leftPadding + rightPadding
    implicitHeight: 20 + topPadding + bottomPadding

    layer.enabled: !control.enabled
    opacity: control.enabled ? 1.0 : 0.3

    background: null

    Rectangle
    {
        id: uncheckedLayer

        width: control.availableWidth
        height: control.availableHeight
        anchors.centerIn: parent

        radius: height / 2
        color: control.uncheckedColor

        Rectangle
        {
            id: uncheckedindicator

            width: parent.height / 2 + 2
            height: width
            radius: height / 2
            x: parent.width - parent.height / 2 - width / 2
            anchors.verticalCenter: parent.verticalCenter
            color: control.uncheckedIndicatorColor

            Rectangle
            {
                width: parent.width - 4
                height: width
                radius: height / 2
                anchors.centerIn: parent
                color: control.uncheckedColor
            }
        }

        Rectangle
        {
            id: checkedLayer

            anchors.fill: parent
            radius: height / 2
            color: control.checkedColor

            Rectangle
            {
                id: checkedIndicator

                width: 2
                height: parent.height / 2
                x: parent.height / 2
                anchors.verticalCenter: parent.verticalCenter
                color: control.checkedIndicatorColor
            }

            opacity: control.checked ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: control.animationDuration } }
        }

        Rectangle
        {
            id: handle

            width: parent.height - 4
            height: width
            radius: height / 2
            color: control.handleColor
            anchors.verticalCenter: parent.verticalCenter

            x: control.checked ? parent.width - width - 2 : 2
            Behavior on x { NumberAnimation { duration: control.animationDuration } }
        }
    }

    contentItem: null

    MouseArea
    {
        anchors.fill: parent
        onClicked:
        {
            control.checked = !control.checked
            control.clicked()
        }
    }
}
