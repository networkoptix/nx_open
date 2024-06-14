// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

Item
{
    id: control

    property bool enabled: true
    property bool isPassword: false
    property string text
    property alias font: labelTextField.font
    property alias color: labelTextField.color
    readonly property bool hovered: labelMouseArea.containsMouse || clipboardCopyButton.hovered
    property int copiedTooltipLifetimeMs: 0

    implicitHeight: labelTextField.implicitHeight

    GlobalToolTip.text: labelTextField.truncated ? text : ""

    Text
    {
        id: labelTextField
        elide: Text.ElideRight
        width: Math.min(control.width - clipboardCopyButton.width - 4, implicitWidth)
        text: isPassword ? "******" : control.text

        MouseArea
        {
            id: labelMouseArea
            anchors.fill: parent
            anchors.rightMargin: - (clipboardCopyButton.anchors.leftMargin + clipboardCopyButton.width)
            hoverEnabled: true
            onClicked: clipboardCopyButton.copy()
        }
    }

    ClipboardCopyButton
    {
        id: clipboardCopyButton
        textFieldReference: control
        anchors.left: labelTextField.right
        anchors.leftMargin: 4
        anchors.verticalCenter: labelTextField.verticalCenter
        visible: control.hovered || animationRunning
        onAnimationRunningChanged:
        {
            if (animationRunning && copiedTooltipLifetimeMs > 0)
                control.GlobalToolTip.showTemporary(qsTr("copied"), copiedTooltipLifetimeMs)
        }
    }
}
