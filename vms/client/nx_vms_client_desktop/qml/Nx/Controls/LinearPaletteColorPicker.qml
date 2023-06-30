// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls.impl 2.3

import Nx.Core 1.0

Item
{
    id: colorPicker

    property alias colors: colorsRepeater.model
    property color color

    property color checkColor: ColorTheme.colors.dark7

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    Row
    {
        id: row
        spacing: 4

        Repeater
        {
            id: colorsRepeater

            Rectangle
            {
                readonly property bool current: colorPicker.color === color

                width: 24
                height: 24
                radius: 2
                color: modelData

                ColorImage
                {
                    anchors.centerIn: parent
                    source: "image://svg/skin/buttons/ok_20.svg"
                    color: checkColor
                    visible: parent.current
                }

                MouseArea
                {
                    anchors.fill: parent
                    onClicked: colorPicker.color = color
                }
            }
        }
    }
}
