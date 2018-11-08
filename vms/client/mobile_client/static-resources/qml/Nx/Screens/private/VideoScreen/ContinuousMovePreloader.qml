import QtQuick 2.6
import QtQuick.Window 2.0
import Nx 1.0

BasePreloader
{
    id: control

    property color color: ColorTheme.transparent(ColorTheme.windowText, 0.9)
    property real sideSize: control.height / 2.5
    property real thickness: 6
    property int figuresCount: 5

    implicitWidth: implicitHeight * 1.5
    implicitHeight: 96

    contentItem: Item
    {
        anchors.fill: parent

        Repeater
        {
            id: repeater

            model: figuresCount

            Item
            {
                id: figure

                property real xPosition: 0
                readonly property real figureWidth:
                    (control.sideSize + control.thickness) * Math.cos(Math.PI / 4) / 2

                x: xPosition - figureWidth
                width: control.sideSize
                height: control.sideSize
                anchors.verticalCenter: parent.verticalCenter
                rotation: 45
                opacity: 0

                Rectangle
                {
                    width: parent.width
                    height: control.thickness

                    color: control.color
                }

                Rectangle
                {
                    y: control.thickness
                    x: parent.width - width
                    width: control.thickness
                    height: control.sideSize - control.thickness

                    color: control.color
                }

                ParallelAnimation
                {
                    loops: Animation.Infinite
                    running: true

                    NumberAnimation
                    {
                        target: figure
                        property: "xPosition"
                        from: index * figure.width / 1.8
                        to: (index + 1) * figure.width / 1.8
                        duration: 450
                    }

                    NumberAnimation
                    {
                        target: figure
                        property: "opacity"
                        from: index === 0 ? 0.0 : 1.0
                        to: index === repeater.count - 1 ? 0.0 : 1.0
                        easing.type: index === 0 ? Easing.InQuad : Easing.OutQuad
                        duration: index === 0 ? 250 : 450
                    }
                }
            }
        }
    }
}
