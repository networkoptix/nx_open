// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Core

import "private"

SwipeView
{
    id: control

    leftPadding: 20
    rightPadding: 20
    topPadding: closeButton.y + closeButton.height + 8
    bottomPadding: promoPageButtons.height

    interactive: false

    signal closeRequested()

    background: Rectangle
    {
        id: backgroundRect

        color: ColorTheme.transparent(ColorTheme.colors.dark5, 0.9)
        radius: 4

        TextButton
        {
            id: closeButton

            anchors.top: backgroundRect.top
            anchors.topMargin: 20
            anchors.right: backgroundRect.right
            anchors.rightMargin: 20

            width: 24
            height: 24

            icon.source: "image://svg/skin/text_buttons/cross_close_20.svg"
            icon.width: closeButton.width
            icon.height: closeButton.height
            color: pressed || hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light9

            onClicked: control.closeRequested()
        }

        PromoPageButtons
        {
            id: promoPageButtons

            pages: control
            width: backgroundRect.width
            anchors.bottom: backgroundRect.bottom

            onCloseRequested: control.closeRequested()
        }
    }

    Binding
    {
        target: contentItem
        property: "clip"
        value: true
    }
}
