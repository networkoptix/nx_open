// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0

import nx.vms.client.core.analytics 1.0 as Analytics

Flow
{
    id: control

    property Analytics.Attribute attribute

    property var selectedValue

    spacing: 2

    Repeater
    {
        model: CoreUtils.toArray(attribute && attribute.colorSet && attribute.colorSet.items)

        OptionButton
        {
            id: button

            text: modelData
            wrapMode: Text.NoWrap

            height: 24
            topPadding: 4
            bottomPadding: 4
            leftPadding: 26

            selected: modelData === control.selectedValue

            onClicked:
                control.selectedValue = selected ? undefined : modelData

            Rectangle
            {
                x: 8
                width: 14
                height: 14
                anchors.verticalCenter: button.verticalCenter
                radius: 1
                color: attribute.colorSet.color(modelData)
                border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
            }
        }
    }
}
