// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Mobile.Controls

import Qt5Compat.GraphicalEffects

Control
{
    id: control

    property bool enableValueControls: true
    property bool showCentralArea: false
    property alias centralArea: centralArea.contentItem

    property alias upButton: upButtonControl
    property alias downButton: downButtonControl

    component ValueButton: Button
    {
        type: Button.Type.LightInterface
        radius: 0
        padding: 0
        enabled: control.enableValueControls
        opacity: enabled ? 1 : 0.3
        icon.width: 24
        icon.height: 24
    }

    contentItem: Column
    {
        spacing: 2

        ValueButton
        {
            id: upButtonControl

            width: 52
            height: 56
            topPadding: 2
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Control
        {
            id: centralArea

            width: 52
            height: 48
            visible: control.showCentralArea
            anchors.horizontalCenter: parent.horizontalCenter
        }

        ValueButton
        {
            id: downButtonControl

            width: 52
            height: 56
            bottomPadding: 2
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    layer.enabled: true
    layer.effect: OpacityMask
    {
        maskSource: Rectangle
        {
            width: control.width
            height: control.height
            radius: 24
        }
    }
}
