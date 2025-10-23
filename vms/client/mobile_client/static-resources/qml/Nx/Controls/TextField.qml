// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window
import QtQuick.Controls

import Nx.Core
import Nx.Controls

import nx.vms.client.core

import "private"

TextField
{
    id: control

    property bool showError: false

    property color inactiveColor: showError ? ColorTheme.colors.red_d1 : ColorTheme.colors.dark7
    property color activeColor: showError ? ColorTheme.colors.red_core : ColorTheme.colors.brand_core
    property color backgroundColor: ColorTheme.colors.dark7
    property real borderWidth: 2
    placeholderTextColor: ColorTheme.colors.light15
    selectByMouse: !CoreUtils.isMobilePlatform()
    color: showError ? ColorTheme.colors.red_core : ColorTheme.colors.light1
    selectionColor: ColorTheme.colors.brand_core

    leftPadding: 8
    rightPadding: 8
    height: 48
    verticalAlignment: TextInput.AlignVCenter

    font.pixelSize: 16
    opacity: enabled ? 1.0 : 0.3

    property real backgroundRightOffset: 0

    background: Item
    {
        Rectangle
        {
            anchors.fill: parent
            color: control.backgroundColor
            radius: 8
            border.color: control.activeFocus ? activeColor : inactiveColor
            border.width: control.borderWidth
        }
    }

    Component.onCompleted:
    {
        if (Qt.platform.os == "android")
            passwordCharacter = "\u2022"
    }
}
