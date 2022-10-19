// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx 1.0

Item
{
    id: promoPage

    property alias title: titleItem.text
    property alias imageUrl: imageItem.source
    property alias text: textItem.text

    property int verticalAlignment: Qt.AlignTop

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    Column
    {
        id: content

        spacing: 8

        width: promoPage.width

        anchors.top: (promoPage.verticalAlignment & Qt.AlignTop) || !promoPage.verticalAlignment
            ? promoPage.top
            : undefined

        anchors.bottom: promoPage.verticalAlignment & Qt.AlignBottom
            ? promoPage.bottom
            : undefined

        anchors.verticalCenter: promoPage.verticalAlignment & Qt.AlignVCenter
            ? promoPage.verticalCenter
            : undefined

        Text
        {
            id: titleItem

            topPadding: 8
            bottomPadding: imageItem.source ? 0 : 8
            width: promoPage.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap

            font.weight: Font.Medium
            font.pixelSize: 16

            color: ColorTheme.colors.light4
            visible: !!text
        }

        Image
        {
            id: imageItem

            sourceSize: Qt.size(240, 120)
            width: promoPage.width
            horizontalAlignment: Text.AlignHCenter
            fillMode: Image.Pad
            visible: source != ""
        }

        Text
        {
            id: textItem

            width: promoPage.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap

            font.weight: Font.Normal
            font.pixelSize: 13

            color: ColorTheme.colors.light4
            visible: !!text
        }
    }
}