// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0
import Nx.Controls 1.0

Control
{
    id: control

    property bool active: false
    property alias text: label.text
    property alias textColor: label.color

    signal clicked()

    implicitWidth: parent ? parent.width : 200
    implicitHeight: 48
    leftPadding: 16
    rightPadding: 16

    font.pixelSize: 16

    background: Item
    {
        Rectangle
        {
            anchors.fill: parent
            visible: control.activeFocus || control.active
            color: ColorTheme.colors.brand_core
            opacity: enabled ? 1.0 : 0.2
        }

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            clip: true
            highlightColor: control.active ? ColorTheme.colors.brand_core : ColorTheme.colors.dark6
            rippleSize: 160
        }
    }

    MouseArea
    {
        id: mouseArea
        parent: control
        anchors.fill: control

        property bool pressedAndHeld: false
        onClicked: control.clicked()
        onPressAndHold: { pressedAndHeld = true }
        onCanceled: { pressedAndHeld = false }
        onReleased:
        {
            if (!pressedAndHeld)
                return

            pressedAndHeld = false
            control.clicked()
        }
    }

    Text
    {
        id: label

        anchors.verticalCenter: parent.verticalCenter
        x: parent.leftPadding
        width: parent.availableWidth
        font: control.font
        color: ColorTheme.colors.brand_contrast
        opacity: enabled || control.active ? 1.0 : 0.2
    }

    Keys.onEnterPressed: clicked()
    Keys.onReturnPressed: clicked()
}
