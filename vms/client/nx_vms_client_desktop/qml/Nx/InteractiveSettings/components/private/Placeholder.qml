// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx

/**
 * Interactive Settings private type.
 */
Control
{
    property alias header: header.text
    property alias description: description.text
    property alias imageSource: image.source
    readonly property real positionRatio: 2/3

    contentItem: ColumnLayout
    {
        spacing: 0

        Item
        {
            Layout.fillHeight: true
            Layout.preferredHeight: positionRatio
        }

        Image
        {
            id: image

            Layout.preferredHeight: 64
            Layout.preferredWidth: 64
            Layout.alignment: Qt.AlignCenter
        }

        Text
        {
            id: header

            color: ColorTheme.colors.dark17
            visible: text
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 15
            lineHeightMode: Text.FixedHeight
            lineHeight: 20
            wrapMode: Text.Wrap

            Layout.alignment: Qt.AlignCenter
            Layout.maximumWidth: parent.width
            Layout.topMargin: 16
        }

        Text
        {
            id: description

            color: ColorTheme.colors.dark17
            visible: text
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 13
            lineHeightMode: Text.FixedHeight
            lineHeight: 16
            wrapMode: Text.Wrap

            Layout.alignment: Qt.AlignCenter
            Layout.maximumWidth: parent.width
            Layout.topMargin: 8
        }

        Item
        {
            Layout.fillHeight: true
            Layout.preferredHeight: 1 //< Position ratio.
        }
    }
}
