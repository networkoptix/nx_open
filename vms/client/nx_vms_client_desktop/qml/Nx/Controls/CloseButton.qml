import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4 as T

import Nx 1.0

AbstractButton
{
    id: button

    property alias radius: rectangle.radius

    palette
    {
        button: "transparent" //< Normal background.
        buttonText: ColorTheme.windowText //< Normal foreground.

        light: ColorTheme.transparent(ColorTheme.windowText, 0.1) //< Hovered background.
        brightText: ColorTheme.windowText //< Hovered foreground.

        window: ColorTheme.transparent(ColorTheme.colors.dark1, 0.1) //< Pressed background.
        windowText: ColorTheme.windowText //< Pressed foreground.
    }

    icon.source: "qrc:/skin/text_buttons/selectable_button_close.png"
    icon.color: down ? palette.windowText : (hovered ? palette.brightText : palette.buttonText)

    background: Rectangle
    {
        id: rectangle

        radius: 2
        color: down ? palette.window : (hovered ? palette.light : palette.button)
    }

    contentItem: T.IconImage
    {
        id: image

        name: button.icon.name
        color: button.icon.color
        source: button.icon.source
    }
}
