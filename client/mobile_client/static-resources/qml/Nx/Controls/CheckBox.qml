import QtQuick 2.6
import QtQuick.Window 2.0
import QtQuick.Controls 2.4
import Nx 1.0

CheckBox
{
    id: control

    font.pixelSize: 16
    spacing: 16

    label: Text
    {
          x: indicator.x + indicator.width + control.spacing
          y: control.topPadding
          width: control.availableWidth - indicator.width - control.spacing
          height: control.availableHeight

          text: control.text
          font: control.font
          color: ColorTheme.windowText
          elide: Text.ElideRight
          visible: control.text
          horizontalAlignment: Text.AlignLeft
          verticalAlignment: Text.AlignVCenter
    }

    indicator: Rectangle
    {
        id: indicator

        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 18
        height: 18

        color: control.checked ? ColorTheme.windowText : "transparent"
        border.color: ColorTheme.windowText
        border.width: 2
        radius: 2

        Canvas
        {
            opacity: control.checked ? 1.0 : 0.0

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
                ctx.strokeStyle = ColorTheme.windowBackground

                ctx.moveTo(2.5, 8.5)
                ctx.lineTo(7, 12.5)
                ctx.lineTo(15, 4.5)
                ctx.stroke()
            }
        }
    }
}

