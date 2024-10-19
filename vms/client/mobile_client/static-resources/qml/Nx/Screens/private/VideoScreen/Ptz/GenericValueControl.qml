// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0

Rectangle
{
    id: control

    property bool enableValueControls: true
    property bool showCentralArea: false
    property alias centralAreaText: centralText.text
    property Item centralArea: null

    property alias upButton: upButtonControl
    property alias downButton: downButtonControl

    color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.8)

    implicitWidth: 56
    implicitHeight: 136

    radius: 28

    MouseArea
    {
        id: clickEventsOmitter

        anchors.fill: parent
    }

    IconButton
    {
        id: upButtonControl

        width: control.width
        height: width
        padding: 0
        enabled: control.enableValueControls
        opacity: enabled ? 1 : 0.3
    }

    Text
    {
        id: centralText

        anchors.centerIn: parent
        visible: !control.showCentralArea
        opacity: 0.5
        font: Qt.font({pixelSize: 12, weight: Font.Bold})
        color: ColorTheme.colors.light16
    }

    IconButton
    {
        id: downButtonControl

        width: control.width
        height: width
        anchors.bottom: parent.bottom
        padding: 0
        enabled: control.enableValueControls
        opacity: enabled ? 1 : 0.3
    }

    onCentralAreaChanged:
    {
        centralArea.parent = control
        centralArea.anchors.centerIn = control
        centralArea.visible = Qt.binding(function() { return control.showCentralArea })
    }
}
