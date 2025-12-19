// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

// A container for Nx.Mobile.Controls/Button components to group them in a short toolbar.
Container
{
    id: control

    spacing: 1

    property color backgroundColor: ColorTheme.colors.dark8
    property real roundingRadius: 6 //< Matches default Button background.

    property Component spacerDelegate: Component
    {
        Rectangle
        {
            width: 1
            height: 32
            color: ColorTheme.colors.dark10
        }
    }

    background: Rectangle
    {
        color: control.backgroundColor
        radius: control.roundingRadius
    }

    contentItem: Item
    {
        id: buttonBarContent

        implicitWidth: buttonRow.width
        implicitHeight: buttonRow.height

        Row
        {
            id: buttonRow

            anchors.centerIn: parent
            spacing: control.spacing

            Repeater
            {
                model: control.contentModel
            }
        }

        Item
        {
            id: separators

            anchors.fill: buttonRow
            z: -1

            Repeater
            {
                model: control.spacerDelegate ? control.count : 0

                delegate: Component
                {
                    Item
                    {
                        id: spacerHolder

                        readonly property Item button: control.itemAt(index)

                        readonly property bool relevant: button.Positioner.index >= 0
                            && !button.Positioner.isLastItem

                        x: control.LayoutMirroring.enabled
                            ? button.x - width
                            : button.x + button.width

                        width: control.spacing
                        height: control.contentItem?.height ?? 0
                        visible: relevant

                        Loader
                        {
                            active: spacerHolder.relevant
                            anchors.centerIn: spacerHolder
                            sourceComponent: control.spacerDelegate
                        }
                    }
                }
            }
        }
    }
}
