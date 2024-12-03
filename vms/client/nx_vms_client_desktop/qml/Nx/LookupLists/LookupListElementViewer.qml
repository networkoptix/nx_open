// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.core.analytics as Analytics

Control
{
    id: control

    required property Analytics.ObjectType objectType
    required property Analytics.Attribute attribute
    required property string displayValue
    required property string value
    required property string colorHexValue

    property bool isGeneric: !objectType

    component ElementViewer: BasicTableCellDelegate
    {
        x: 8
        y: 6
        color: ColorTheme.light
    }

    Component
    {
        id: textDelegate

        ElementViewer
        {
            text: displayValue
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
            text: displayValue

            Rectangle
            {
                x: 8
                width: 14
                height: 14
                radius: 1
                anchors.verticalCenter: parent.verticalCenter
                border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
                color: colorHexValue || "transparent"
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
