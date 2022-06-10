// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Nx 1.0

ColumnLayout
{
    id: settingsPlaceholder

    property alias header: header.text
    property alias description: description.text
    property alias imageSource: image.source
    readonly property real positionRatio: 2/3

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
