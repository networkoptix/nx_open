// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Controls 1.0

Item
{
    id: control

    property var values: []

    implicitWidth: Math.max(flow.implicitWidth, 1)
    implicitHeight: Math.max(flow.implicitHeight, 1)

    Flow
    {
        id: flow

        width: control.width
        height: implicitHeight

        spacing: 4

        Repeater
        {
            model: control.values

            delegate: Component
            {
                SelectableTextButton
                {
                    text: modelData
                    selectable: false
                    hoverEnabled: false
                    deactivatable: true
                    width: Math.min(implicitWidth, flow.width)

                    onDeactivated:
                    {
                        // A weird way to remove an element from potentially a list property.
                        control.values = Array.prototype.filter.call(control.values,
                            (value, idx) => idx !== index)
                    }
                }
            }
        }
    }
}
