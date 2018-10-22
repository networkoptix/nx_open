import QtQuick 2.11
import Nx 1.0

Rectangle
{
    property var control: null

    color:
    {
        const color = ColorTheme.shadow

        if (control && control.readOnly)
            return ColorTheme.transparent(ColorTheme.shadow, 0.2)

        if (control && control.activeFocus)
            return ColorTheme.darker(color, 1)

        return ColorTheme.shadow
    }

    border.color:
    {
        const color = ColorTheme.shadow

        if (control && control.readOnly)
            return ColorTheme.transparent(color, 0.4)

        if (control && control.activeFocus)
            return ColorTheme.darker(color, 3)

        return ColorTheme.darker(color, 1)
    }

    radius: 1

    Rectangle
    {
        width: parent.width
        height: 2
        color: parent.border.color
        visible: control && control.activeFocus
    }

    opacity: enabled ? 1.0 : 0.3
}
