// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Window 2.0

import Nx.Core 1.0

import "private"

CheckBox
{
    id: control

    property alias backgoundColor: backgroundItem.color
    property alias iconSource: icon.source

    implicitWidth: leftPadding + rightPadding + indicator.implicitwidth + contentItem.implicitWidth
    implicitHeight: topPadding + bottomPadding + textItem.implicitHeight

    font.pixelSize: 16

    topPadding: 16
    bottomPadding: 12
    spacing: 8
    leftPadding: 16
    rightPadding: 16

    opacity: control.enabled ? 1.0 : 0.3

    background: Rectangle
    {
        id: backgroundItem
        color: ColorTheme.colors.dark6
    }

    indicator: CheckIndicator
    {
        anchors.right: parent.right
        anchors.rightMargin: control.rightPadding
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

    contentItem: Row
    {
        spacing: control.spacing

        Image
        {
            id: icon

            x: control.leftPadding
            width: 24
            height: 24
            anchors.verticalCenter: parent.verticalCenter
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
            visible: status === Image.Ready
        }

        Text
        {
            id: textItem

            text: control.text
            font: control.font
            color: ColorTheme.windowText
            width: parent.width - (icon.x + icon.width + indicator.width + 2 * control.spacing)
            wrapMode: Text.Wrap
        }
    }
}
