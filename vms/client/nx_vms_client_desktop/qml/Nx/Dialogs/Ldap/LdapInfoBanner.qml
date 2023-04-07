// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core 1.0

Rectangle
{
    property alias text: bannerText.text

    color: "#17324c"
    border.color: "#215385"
    border.width: 1

    implicitHeight: bannerText.height

    Image
    {
        anchors
        {
            left: parent.left
            leftMargin: 16
            top: parent.top
            topMargin: 10
        }

        source: "image://svg/skin/user_settings/ldap_banner_info.svg"
        sourceSize: Qt.size(20, 20)
    }

    Text
    {
        id: bannerText

        anchors
        {
            left: parent.left
            leftMargin: 44
            right: parent.right
            rightMargin: 16
        }
        topPadding: 12
        bottomPadding: 12

        color: ColorTheme.colors.light4
        font: { pixelSize: 14; weight: Font.Medium}
    }
}
