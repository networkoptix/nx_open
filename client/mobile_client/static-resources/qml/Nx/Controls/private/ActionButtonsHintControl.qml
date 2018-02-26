import QtQuick 2.6
import Nx 1.0

Rectangle
{
    id: control

    property string icon

    property int horizontalPadding: 12
    property int verticalPadding: 12

    width: bodyRow.width + horizontalPadding
    height: bodyRow.height + verticalPadding * 2
    color: ColorTheme.transparent(ColorTheme.base8, 0.95)
    radius: 2

    visible: false

    Component
    {
        id: dummyComponent

        Item {}
    }

    Component
    {
        id: textComponent

        Text
        {
            height: 24
            wrapMode: Text.NoWrap
            verticalAlignment: Text.AlignVCenter
            color: ColorTheme.brightText
            font.pixelSize: 14
            font.weight: Font.Normal
        }

    }

    function showHint(text, iconPath)
    {
        hideTimer.restart()

        loader.sourceComponent = textComponent
        loader.item.text = text
        control.icon = iconPath
        control.visible = true
    }

    function showCustomProcess(component, iconPath)
    {
        hideTimer.stop()

        loader.sourceComponent = component
        control.icon = iconPath
        control.visible = true
    }

    function hide()
    {
        loader.sourceComponent = dummyComponent
        hideTimer.stop()
        control.visible = false
    }

    Row
    {
        id: bodyRow

        x: control.horizontalPadding
        y: control.verticalPadding
        height: 24

        Loader
        {
            id: loader
            anchors.verticalCenter: parent.verticalCenter
        }

        Image
        {
            source: lp(icon)
            visible: icon.length
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Timer
    {
        id: hideTimer

        interval: 3000
        onTriggered: control.visible = false
    }
}
