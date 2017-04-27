import QtQuick 2.6
import Nx 1.0

Rectangle
{
    id: control

    property bool showCenterArea: false
    property int actionButtonHeight: 56 //< Just some default value

    property alias upPressed: upButton.pressed
    property alias downPressed: downButton.pressed


    property Component upButtonDecoration
    property Component downButtonDecoration
    property Component centralAreaDelegate

    color: ColorTheme.transparent(ColorTheme.base8, 0.8)

    implicitWidth: 56
    implicitHeight: showCenterArea ? 136 : 112

    radius: 28

    RoundButton
    {
        id: upButton

        y: spotRadius - height / 2
        width: control.width
        height: actionButtonHeight

        Loader
        {
            anchors.centerIn: parent
            sourceComponent: control.upButtonDecoration
        }
    }

    Loader
    {
        id: centerAreaLoader

        z: 1
        anchors.centerIn: parent
        sourceComponent: control.centralAreaDelegate
        visible: control.showCenterArea
    }

    RoundButton
    {
        id: downButton

        y: parent.height - (spotRadius + height / 2)
        width: control.width
        height: actionButtonHeight

        Loader
        {
            anchors.centerIn: parent
            sourceComponent: control.downButtonDecoration
        }
    }
}

