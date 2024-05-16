// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

Control
{
    id: control

    required property Analytics.ObjectType objectType
    required property Analytics.Attribute attribute
    required property string displayValue
    required property string value

    property bool isGeneric: !objectType

    component ElementViewer: BasicTableCellDelegate
    {
        x: 8
        y: 6
        color: displayValue ? ColorTheme.light : ColorTheme.colors.dark17
    }

    Component
    {
        id: textDelegate

        ElementViewer
        {
            text: displayValue || qsTr("ANY")
        }
    }

    Component
    {
        id: colorDelegate

        ElementViewer
        {
            anchors.fill: parent
            leftPadding: 26
            rightPadding: 8
            verticalAlignment: Text.AlignVCenter
            text: displayValue || qsTr("ANY color")

            Rectangle
            {
                x: 8
                width: 14
                height: 14
                radius: 1
                anchors.verticalCenter: parent.verticalCenter
                border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
                color: Utils.getValue(value, "transparent")
            }
        }
    }

    function calculateComponent()
    {
        if (isGeneric || !attribute)
            return textDelegate

        if (attribute.type === Analytics.Attribute.Type.colorSet)
            return colorDelegate
        return textDelegate
    }

    contentItem: Loader
    {
        id: loader
        sourceComponent: calculateComponent()
    }
}
