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

    property int radius: 24
    property bool overlayStyle: false

    implicitWidth: 52
    implicitHeight: 164

    component ValueButton: PtzButton
    {
        overlayStyle: control.overlayStyle
        borderColor: "transparent"
        enabled: control.enableValueControls
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

    Rectangle
    {
        id: border

        anchors.fill: parent

        visible: control.overlayStyle

        radius: control.radius
        color: "transparent"
        border.width: 1
        border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
    }

    layer.enabled: true
    layer.effect: OpacityMask
    {
        maskSource: Rectangle
        {
            width: control.width
            height: control.height
            radius: control.radius
        }
    }
}
