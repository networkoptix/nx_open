// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

/**
 * Interactive Settings type.
 */
Control
{
    property string icon: "info"
    property alias text: label.text

    padding: 12
    property bool fillWidth: true

    background: Rectangle
    {
        color: ColorTheme.colors.dark8
        radius: 2
    }

    contentItem: RowLayout
    {
        spacing: 8
        baselineOffset: label.y + label.baselineOffset

        Image
        {
            id: image

            source: "qrc:///skin/analytics_icons/settings/%1.png".arg(icon)

            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignTop
        }

        Label
        {
            id: label

            font.pixelSize: 12
            lineHeightMode: Text.FixedHeight
            lineHeight: 16
            wrapMode: Text.Wrap
            linkColor: hoveredLink
                ? ColorTheme.colors.brand_core
                : ColorTheme.colors.brand_d2

            onLinkActivated: (link) => Qt.openUrlExternally(link)

            Layout.fillWidth: true
        }
    }
}
