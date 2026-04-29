// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    implicitWidth: 52
    implicitHeight: 164

    component ValueButton: Button
    {
        type: Button.Type.LightInterface
        foregroundColor: ColorTheme.colors.light4
        background.opacity: 0.6
        radius: 0
        padding: 0
        enabled: control.enableValueControls
        opacity: enabled ? 1 : 0.3
        icon.width: 24
        icon.height: 24
    }

    contentItem: ColumnLayout
    {
        spacing: 2

        ValueButton
        {
            id: upButtonControl

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 56

            topPadding: 2
        }

        Control
        {
            id: centralArea

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 48

            visible: control.showCentralArea
        }

        ValueButton
        {
            id: downButtonControl

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 56

            bottomPadding: 2
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
