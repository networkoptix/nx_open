import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

Rectangle
{
    id: control

    property string icon

    property int horizontalPadding: 12
    property int verticalPadding: 12

    width: bodyRow.width + horizontalPadding * 2
    height: bodyRow.height + verticalPadding * 2
    color: ColorTheme.transparent(ColorTheme.base8, 0.95)
    radius: 2

    visible: false

    function show(text, iconPath)
    {
        control.icon = iconPath
        hintTextControl.text = text
        hideTimer.restart()
        control.visible = true
    }

    function hide()
    {
        hideTimer.stop()
        control.visible = false
    }

    Row
    {
        id: bodyRow

        x: control.horizontalPadding
        y: control.verticalPadding
        height: 24
        spacing: 12

        VoiceSpectrumItem
        {
            id: vsi

            height: 24
            width: 96
        }

        Text
        {
            id: hintTextControl
            height: 24
            wrapMode: Text.NoWrap
            verticalAlignment: Text.AlignVCenter
            color: ColorTheme.brightText
            font.pixelSize: 14
            font.weight: Font.Normal
        }

        Image
        {
            source: lp(icon)
            visible: icon.length
        }
    }

    Timer
    {
        id: hideTimer

        interval: 3000
        onTriggered: control.visible = false
    }
}
