// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15 as QuickControls

import Nx.Core 1.0
import Nx.Controls 1.0

QuickControls.TextField
{
    id: control

    property alias labelText: label.text

    color: ColorTheme.colors.light4
    font.pixelSize: 16
    implicitHeight: 56
    topPadding: 30
    leftPadding: 16
    rightPadding: clearButton.visible
        ? clearButton.width
        : 16

    background: Item
    {
        implicitWidth: label.implicitWidth

        Rectangle
        {
            id: bottomCurtain

            width: parent.width
            height: 2 * mainBackground.radius
            anchors.bottom: parent.bottom
            color: mainBackground.color
            visible: underline.visible
        }

        Rectangle
        {
            id: mainBackground

            radius: 8
            anchors.fill: parent
            color: control.focus
                ? ColorTheme.colors.dark8
                : ColorTheme.colors.dark7
        }

        Rectangle
        {
            id: underline

            width: parent.width
            height: 2
            anchors.bottom: parent.bottom
            color: ColorTheme.colors.brand_core
            visible: control.focus
        }

        Text
        {
            id: label

            x: control.leftPadding
            width: parent.width - x - control.rightPadding
            y: control.text || control.focus ? 8 : 18

            font.pixelSize: control.text || control.focus ? 12 : 16
            color: ColorTheme.colors.light16
            elide: Text.ElideRight
        }

    }

    IconButton
    {
        id: clearButton

        width: 56
        height: width
        padding: 4
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        visible: control.text

        icon.source: lp("/images/clear_text.svg")
        icon.width: 20
        icon.height: icon.width

        onClicked:
        {
            control.clear()
            control.forceActiveFocus()
        }
    }
}
