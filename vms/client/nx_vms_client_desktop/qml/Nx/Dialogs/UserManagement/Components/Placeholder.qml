// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls

ColumnLayout
{
    id: placeholder

    property alias imageSource: placeholderImage.sourcePath
    property alias text: placeholderMainText.text
    property alias additionalText: placeholderAdditionalText.text

    property color textColor: ColorTheme.colors.light16

    anchors.centerIn: parent
    anchors.verticalCenterOffset: -100

    property real maxWidth: parent.width - 16 * 2

    spacing: 8

    SvgImage
    {
        id: placeholderImage

        Layout.alignment: Qt.AlignHCenter

        width: 64
        height: 64

        primaryColor: textColor
        sourceSize: Qt.size(width, height)
    }

    Text
    {
        id: placeholderMainText

        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8
        Layout.maximumWidth: placeholder.maxWidth

        font: Qt.font({pixelSize: 16, weight: Font.Medium})
        color: placeholder.textColor
    }

    Text
    {
        id: placeholderAdditionalText
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: placeholder.maxWidth

        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: placeholder.textColor
    }
}
