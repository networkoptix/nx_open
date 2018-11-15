import QtQuick 2.0
import QtQuick.Controls 2.0
import Nx 1.0

ScrollBar
{
    id: control

    padding: 0

    visible: policy === ScrollBar.AlwaysOn || (policy === ScrollBar.AsNeeded && size < 1.0)

    background: Rectangle
    {
        color: ColorTheme.dark
    }

    contentItem: Rectangle
    {
        implicitWidth: 8
        implicitHeight: 8

        color:
        {
            const color = ColorTheme.midlight
            if (control.pressed)
                return ColorTheme.lighter(color, 1)
            if (control.hovered)
                return ColorTheme.lighter(color, 2)
            return color
        }
    }
}
