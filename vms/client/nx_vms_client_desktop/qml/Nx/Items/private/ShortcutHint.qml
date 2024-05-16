// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

Item
{
    id: control

    property var keys: ["Ctrl", "Enter"] //< A sample default value.
    property alias description: descriptionText.text

    property color color: ColorTheme.colors.dark14
    property font font

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    Row
    {
        id: content
        spacing: 8

        Row
        {
            id: keySequence

            spacing: 4

            Repeater
            {
                model: control.keys

                Row
                {
                    readonly property bool isLast: Positioner.isLastItem
                    spacing: keySequence.spacing

                    Rectangle
                    {
                        id: frame

                        width: keyText.width + 16
                        color: "transparent"
                        border.color: control.color
                        radius: 2
                        height: 20

                        Text
                        {
                            id: keyText

                            text: modelData
                            height: frame.height
                            verticalAlignment: Text.AlignVCenter
                            color: control.color
                            font: control.font
                            x: 8
                        }
                    }

                    Text
                    {
                        text: "+"
                        visible: !parent.isLast
                        height: frame.height
                        verticalAlignment: Text.AlignVCenter
                        color: control.color
                        font: control.font
                    }
                }
            }
        }

        Text
        {
            id: mdash

            text: "\u2014"
            color: control.color
            font: control.font
            height: keySequence.height
            verticalAlignment: Text.AlignVCenter
        }

        Text
        {
            id: descriptionText

            color: control.color
            font: control.font
            height: keySequence.height
            verticalAlignment: Text.AlignVCenter
        }
    }
}
