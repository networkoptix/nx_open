import QtQuick 2.6

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

    color: "LightSlateGrey" // TODO: set appropriate

    implicitWidth: 56
    implicitHeight: actionButtonHeight * 2
        + (showCenterArea && centerAreaLoader.item ? centerAreaLoader.item.height : 0)

    radius: 28

    Column
    {
        spacing: 0
        anchors.fill: parent

        RoundButton
        {
            id: upButton

            z: 1
            width: control.width
            height: actionButtonHeight

            pressedSpotItemVerticalOffset: spotRadius - (y + height / 2)

            Loader
            {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: parent.pressedSpotItemVerticalOffset
                sourceComponent: control.upButtonDecoration
            }
        }

        Loader
        {
            id: centerAreaLoader

            anchors.horizontalCenter: parent.horizontalCenter
            sourceComponent: control.centralAreaDelegate
            visible: control.showCenterArea
        }

        RoundButton
        {
            id: downButton

            z: 1
            width: control.width
            height: actionButtonHeight
            pressedSpotItemVerticalOffset: parent.height - (spotRadius + y + height / 2)

            Loader
            {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: parent.pressedSpotItemVerticalOffset
                sourceComponent: control.downButtonDecoration
            }
        }
    }
}

