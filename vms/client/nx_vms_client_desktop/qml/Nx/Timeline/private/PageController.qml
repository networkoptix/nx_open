// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core

Control
{
    id: control

    property int total: 0
    onTotalChanged:
    {
        current = total > 0 ? 1 : 0
    }
    property int current: 0

    contentItem: Item
    {
        implicitHeight: contentRow.implicitHeight

        Row
        {
            id: contentRow
            anchors.centerIn: parent
            spacing: 4

            ImageButton
            {
                id: slideLeftButton
                icon.source: "image://skin/16x16/Outline/arrow_left.svg"
                icon.width: 16
                icon.height: 16
                icon.color: ColorTheme.colors.light4
                enabled: control.total !== 0 && control.current !== 1

                onClicked:
                {
                    control.current = MathUtils.bound(1, control.current - 1, control.total)
                }
            }

            Text
            {
                id: pageDisplay
                text: "%1/%2".arg(control.current).arg(control.total)
                color: ColorTheme.colors.light4
            }

            ImageButton
            {
                id: slideRightButton
                icon.source: "image://skin/16x16/Outline/arrow_right.svg"
                icon.width: 16
                icon.height: 16
                icon.color: ColorTheme.colors.light4
                enabled: control.total !== 0 && control.current !== control.total

                onClicked:
                {
                    control.current = MathUtils.bound(1, control.current + 1, control.total)
                }
            }
        }
    }
}
