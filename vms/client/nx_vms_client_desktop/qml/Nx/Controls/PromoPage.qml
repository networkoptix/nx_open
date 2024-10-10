// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: promoPage

    property alias title: titleItem.text
    property alias imageUrl: imageItem.sourcePath
    property alias primaryColor: imageItem.primaryColor
    property alias secondaryColor: imageItem.secondaryColor
    property alias tertiaryColor: imageItem.tertiaryColor
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

        spacing: promoPage.imageUrl == "" ? 16 : 8

        Item
        {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true

            width: 200
            Layout.maximumHeight: 75

            ColoredImage
            {
                id: imageItem

                anchors.fill: parent

                fillMode: Image.PreserveAspectFit

                visible: sourcePath !== ""
            }
        }

        Text
        {
            id: titleItem

            Layout.fillWidth: true

            horizontalAlignment: Text.AlignHCenter

            wrapMode: Text.Wrap
            font.weight: Font.Medium
            font.pixelSize: FontConfig.xLarge.pixelSize

            color: ColorTheme.colors.light4
            visible: !!text
        }

        Text
        {
            id: textItem

            Layout.fillWidth: true
            Layout.leftMargin: 13
            Layout.rightMargin: 13

            horizontalAlignment: Text.AlignHCenter

            wrapMode: Text.Wrap

            font.weight: Font.Normal
            font.pixelSize: FontConfig.normal.pixelSize

            color: promoPage.imageUrl == "" ? ColorTheme.colors.light4 : ColorTheme.colors.light10

            visible: !!text
        }
    }
}
