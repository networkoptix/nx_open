// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

RadioButton
{
    id: control

    property color color: ColorTheme.colors.light4

    font.pixelSize: 16

    icon.width: 24
    icon.height: 24
    icon.color: control.color

    spacing: 8

    leftPadding: 8
    rightPadding: 12
    topPadding: 12
    bottomPadding: 12

    implicitWidth: indicator.width + contentItem.implicitWidth
        + leftPadding + rightPadding + spacing

    implicitHeight: Math.max(indicator.height, contentItem.implicitHeight)
        + topPadding + bottomPadding

    indicator: RadioButtonIndicator
    {
        checked: control.checked
        anchors.right: control.right
        anchors.rightMargin: control.rightPadding
        anchors.verticalCenter: control.verticalCenter
    }

    contentItem: Row
    {
        id: row

        spacing: control.spacing
        baselineOffset: label.y + label.baselineOffset

        ColoredImage
        {
            id: image

            anchors.verticalCenter: row.verticalCenter
            visible: !!sourcePath

            sourcePath: control.icon.source
            sourceSize: Qt.size(control.icon.width, control.icon.height)
            primaryColor: control.icon.color
            name: control.icon.name
        }

        Text
        {
            id: label

            anchors.verticalCenter: row.verticalCenter
            visible: !!text

            text: control.text
            font: control.font
            color: control.color
            elide: Text.ElideRight
        }
    }
}
