// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

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
