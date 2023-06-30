// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0
import Nx.Controls 1.0

Column
{
    id: control

    property alias model: repeater.model
    property alias delegate: repeater.delegate

    property string textRole
    property string valueRole

    property var selectedValue: undefined

    property Component middleItemDelegate

    property string selectedText:
    {
        const item = selectedValue !== undefined
            ? (Array.prototype.find.call(children, item => item.value === selectedValue) || null)
            : null

        return item ? item.text : ""
    }

    spacing: 6

    leftPadding: 2

    Repeater
    {
        id: repeater

        delegate: Component
        {
            RadioButton
            {
                id: button

                function getData(role)
                {
                    return role
                        ? Array.isArray(control.model) ? modelData[role] : model[role]
                        : modelData
                }

                text: getData(control.textRole)
                readonly property var value: getData(control.valueRole)

                checked: value === control.selectedValue

                onClicked:
                    control.selectedValue = value

                middleItem: Loader
                {
                    id: middleItemLoader

                    property alias modelData: button.value //< Accessible from delegate.

                    sourceComponent: control.middleItemDelegate
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
