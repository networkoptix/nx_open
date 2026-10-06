// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core

import "private"

CheckBox
{
    id: control

    property alias backgroundRadius: backgroundItem.radius
    property color backgroundColor: ColorTheme.colors.dark6
    property color checkedBackgroundColor: ColorTheme.colors.dark8
    property alias iconSource: icon.source
    property alias description: description.text

    implicitWidth: leftPadding + rightPadding + indicator.implicitWidth + contentItem.implicitWidth
    implicitHeight: topPadding + bottomPadding + textColumn.implicitHeight

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
        color: control.checked
            ? control.checkedBackgroundColor
            : control.backgroundColor

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            rippleSize: 160
            mouseArea: control
        }
    }

    indicator: CheckIndicator
    {
        anchors.right: parent.right
        anchors.rightMargin: control.rightPadding
        anchors.verticalCenter: parent.verticalCenter
        removeBorderWhenChecked: false
        checked: control.checked
        color: "transparent"
        checkColor: ColorTheme.colors.brand_core
        checkedColor: "transparent"
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

        Column
        {
            id: textColumn

            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - (icon.x + icon.width + indicator.width + 2 * control.spacing)
            spacing: 4

            Text
            {
                id: title

                width: parent.width

                text: control.text
                font: control.font
                textFormat: Text.StyledText
                color: control.checked ? ColorTheme.colors.brand_core : ColorTheme.colors.light10
                elide: Text.ElideRight
            }

            Text
            {
                id: description

                width: parent.width

                visible: !!text
                font.pixelSize: 14
                wrapMode: Text.Wrap
                color: ColorTheme.colors.light12
            }
        }
    }
}
