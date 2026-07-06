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
        implicitHeight: contentColumn.implicitHeight

        Column
        {
            id: contentColumn

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

                topPadding: 24
                leftPadding: 12
                rightPadding: 12

                font.pixelSize: 16
                font.weight: Font.Medium

                horizontalAlignment: Text.AlignHCenter
                color: ColorTheme.colors.light4
                wrapMode: Text.WordWrap
                visible: !!text
            }

            Text
            {
                id: description

                topPadding: 16
                leftPadding: 12
                rightPadding: 12

                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                color: ColorTheme.colors.light12
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
