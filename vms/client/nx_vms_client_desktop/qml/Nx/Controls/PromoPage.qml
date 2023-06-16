// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Core

Item
{
    id: promoPage

    property alias title: titleItem.text
    property alias imageUrl: imageItem.source
    property alias text: textItem.text

    property int verticalAlignment: Qt.AlignTop

    implicitWidth: content.implicitWidth
    implicitHeight: 120

    ColumnLayout
    {
        id: content

        anchors.top: ((promoPage.verticalAlignment & Qt.AlignTop)
            || (promoPage.verticalAlignment & Qt.AlignVCenter)
            || !promoPage.verticalAlignment)
                ? parent.top
                : undefined

        anchors.bottom: ((promoPage.verticalAlignment & Qt.AlignBottom)
            || (promoPage.verticalAlignment & Qt.AlignVCenter))
                ? parent.bottom
                : undefined

        width: promoPage.width

        spacing: 8

        Text
        {
            id: titleItem

            Layout.fillWidth: true

            topPadding: 8
            bottomPadding: imageItem.source ? 0 : 8
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

            Layout.fillWidth: true
            Layout.fillHeight: true

            horizontalAlignment: Text.AlignHCenter

            fillMode: Image.PreserveAspectFit

            visible: source != ""
        }

        Text
        {
            id: textItem

            Layout.fillWidth: true

            horizontalAlignment: Text.AlignHCenter

            wrapMode: Text.Wrap
            font.weight: Font.Normal
            font.pixelSize: 13
            color: ColorTheme.colors.light4

            visible: !!text
        }
    }
}