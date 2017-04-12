import QtQuick 2.6
import QtQuick.Window 2.2
import Nx 1.0

Item
{
    id: indicator

    property bool checked: false
    property color color: ColorTheme.windowText
    property color checkedColor: ColorTheme.brand_main
    property color checkColor: ColorTheme.brand_contrast

    implicitWidth: 18
    implicitHeight: 18

    Rectangle
    {
        anchors.fill: parent

        color: indicator.checked ? indicator.checkedColor : "transparent"
        border.color: indicator.checked ? indicator.checkedColor : indicator.color
        border.width: 2
        radius: 2

        Canvas
        {
            opacity: indicator.checked ? 1.0 : 0.0

            anchors.centerIn: parent
            width: 18 * Screen.devicePixelRatio
            height: 18 * Screen.devicePixelRatio
            scale: 1.0 / Screen.devicePixelRatio

            renderStrategy: Canvas.Cooperative

            onPaint:
            {
                var ctx = getContext('2d')
                ctx.reset()
                ctx.scale(Screen.devicePixelRatio, Screen.devicePixelRatio)

                ctx.lineWidth = 2.5
                ctx.strokeStyle = indicator.checkColor

                ctx.moveTo(2.5, 8.5)
                ctx.lineTo(7, 12.5)
                ctx.lineTo(15, 4.5)
                ctx.stroke()
            }
        }
    }
}
