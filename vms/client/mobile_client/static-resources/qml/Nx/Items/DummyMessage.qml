// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0
import Nx.Controls 1.0

Pane
{
    id: dummy

    property alias title: title.text
    property alias titleColor: title.color
    property alias description: description.text
    property alias descriptionColor: description.color
    property alias descriptionFontPixelSize: description.font.pixelSize
    property alias buttonText: button.text
    property alias image: image.source
    property bool compact: false
    property bool centered: false

    signal buttonClicked()

    padding: 0

    background: Item {}

    contentItem: Item
    {
        Column
        {
            width: parent.width
            y: (parent.height - height) / (centered ? 2 : 4)

            Image
            {
                id: image
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !compact && status == Image.Ready
            }

            Text
            {
                id: title
                width: parent.width
                topPadding: 8
                leftPadding: 16
                rightPadding: 16
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 22
                lineHeight: 1.25
                color: ColorTheme.colors.light16
                wrapMode: Text.WordWrap
                visible: !!text
            }

            Text
            {
                id: description
                topPadding: 8
                leftPadding: 16
                rightPadding: 16
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                lineHeight: 1.25
                color: ColorTheme.colors.dark16
                wrapMode: Text.WordWrap
                visible: !!text
            }

            Button
            {
                id: button

                padding: 16
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !!text
                onClicked: dummy.buttonClicked()
            }
        }
    }
}
