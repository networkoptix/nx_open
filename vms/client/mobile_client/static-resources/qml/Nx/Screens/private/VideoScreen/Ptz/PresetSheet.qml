// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Mobile.Controls

import Nx.Core

AdaptiveSheet
{
    id: sheet

    property alias model: presets.model
    property int highlightIndex: -1

    signal selected(int index)

    contentSpacing: 0

    titleTextItem
    {
        text: qsTr("PTZ Presets")
        color: ColorTheme.colors.light18
        font.pixelSize: 16
        font.weight: Font.Normal
        horizontalAlignment: Text.AlignHCenter
    }

    Repeater
    {
        id: presets

        delegate: Text
        {
            width: parent.width
            height: 54

            text: model.display
            color: index === highlightIndex ? ColorTheme.colors.light4 : ColorTheme.colors.light10
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Rectangle
            {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: -20

                height: 1
                color: ColorTheme.colors.dark12
            }

            MouseArea
            {
                anchors.fill: parent

                onClicked:
                {
                    sheet.selected(index)
                    close()
                }
            }
        }
    }

    footer: Button
    {
        text: qsTr("Close")
        type: Button.LightInterface
        onClicked: close()
    }
}
