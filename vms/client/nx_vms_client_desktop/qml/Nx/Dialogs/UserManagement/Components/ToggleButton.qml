// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15

import Nx 1.0

import nx.vms.client.desktop 1.0

AbstractButton
{
    id: button

    property int flatSide: 1

    width: contentRow.width + 6 + 12

    height: 28

    onClicked: if (!checked) checked = true

    GlobalToolTip.text: buttonText.truncated ? buttonText.text : ""

    background: Item
    {
        clip: true

        Rectangle
        {
            color: button.checked
                ? ColorTheme.colors.dark10
                : ColorTheme.colors.dark5

            radius: 4 * Math.abs(button.flatSide)
            x: flatSide === 0 ? 0 : - radius/2 - radius * button.flatSide / 2
            width: parent.width + radius * Math.abs(button.flatSide)
            height: parent.height
        }
    }

    contentItem: Item
    {
        id: defaultArea //< Area within paddings; actual content may be bigger.

        RowLayout
        {
            id: contentRow
            anchors.centerIn: parent
            spacing: 2

            IconImage
            {
                id: iconImage

                source: button.icon.source
                color: buttonText.color
                sourceSize: Qt.size(button.icon.width, button.icon.height)
            }

            Text
            {
                id: buttonText

                verticalAlignment: Qt.AlignVCenter

                text: button.text
                maximumLineCount: 1
                wrapMode: Text.Wrap
                lineHeight: 15
                lineHeightMode: Text.FixedHeight
                elide: Text.ElideRight

                color: button.checked
                    ? ColorTheme.colors.light4
                    : ColorTheme.colors.light16
            }
        }
    }
}
