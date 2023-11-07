// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

RowLayout
{
    id: header

    property alias name: name.text
    property alias version: version.text
    property alias authCode: authCode.text

    implicitHeight: 20
    spacing: 8

    component RequestAuthCode: Item
    {
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
        baselineOffset: authCode.y + authCode.baselineOffset

        property alias text: authCode.text

        IconImage
        {
            id: image
            source: "image://svg/skin/welcome_screen/tile/lock.svg"
            color: ColorTheme.highlight
        }

        Text
        {
            id: authCode

            anchors.left: image.right
            anchors.margins: 4
            anchors.verticalCenter: image.verticalCenter

            font.pixelSize: 14
            color: ColorTheme.highlight
            visible: !!text
        }
    }

    Text
    {
        id: name

        color: ColorTheme.text
        font.pixelSize: 16
        font.weight: Font.Medium
        elide: Text.ElideRight
        Layout.alignment: Qt.AlignBaseline
    }

    Text
    {
        id: version

        color: ColorTheme.windowText
        font.pixelSize: 14
        visible: !!text
        elide: Text.ElideRight
        Layout.alignment: Qt.AlignBaseline
        Layout.fillWidth: true
    }


    RequestAuthCode
    {
        id: authCode

        Layout.alignment: Qt.AlignBaseline
    }
}
