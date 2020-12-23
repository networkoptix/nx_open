import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import Nx 1.0

import "private"

SwipeView
{
    id: control

    leftPadding: 16
    rightPadding: 16
    topPadding: 8
    bottomPadding: promoPageButtons.height

    interactive: false

    signal closeRequested()

    background: Rectangle
    {
        id: backgroundRect

        color: ColorTheme.transparent(ColorTheme.colors.dark5, 0.9)
        radius: 4

        PromoPageButtons
        {
            id: promoPageButtons

            pages: control
            width: backgroundRect.width
            anchors.bottom: backgroundRect.bottom

            onCloseRequested:
                control.closeRequested()
        }
    }

    Binding
    {
        target: contentItem
        property: "clip"
        value: true
    }
}
