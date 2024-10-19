// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Window 2.0
import QtQuick.Controls 2.4
import Nx.Core 1.0

import "private"

CheckBox
{
    id: control

    font.pixelSize: 16
    spacing: 16
    implicitWidth: leftPadding + rightPadding + contentItem.implicitWidth + indicator.width

    contentItem: Text
    {
        anchors.left: indicator.right
        anchors.right: control.right
        anchors.leftMargin: control.spacing
        anchors.verticalCenter: parent.verticalCenter

        text: control.text
        font: control.font
        color: ColorTheme.colors.brand_contrast
        elide: Text.ElideRight
        visible: control.text
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
    }

    indicator: CheckIndicator
    {
        x: control.leftPadding
        anchors.verticalCenter: parent.verticalCenter
        removeBorderWhenChecked: false
        checked: control.checked
        color: ColorTheme.colors.dark5
        checkColor: ColorTheme.colors.brand_core
        checkedColor: ColorTheme.colors.dark7
        backgroundColor: ColorTheme.colors.dark9
        borderWidth: 1
        lineWidth: 1.5
        checkMarkScale: 1.2
        checkMarkHorizontalOffset: 2
        checkMarkVerticalOffset: -1
    }
}
