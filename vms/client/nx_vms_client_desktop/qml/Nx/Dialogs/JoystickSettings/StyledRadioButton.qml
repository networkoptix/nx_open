// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

RadioButton
{
    id: control

    property string iconSource: ""
    property int logicalId: 0

    height: icon.height

    indicator: Image
    {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        opacity: enabled ? 1.0 : 0.3

        source:
        {
            let source = "qrc:///skin/theme/radiobutton"
            if (control.checked)
                source += "_checked"
            if (control.hovered)
                source += "_hover"
            return source + ".png"
        }
    }

    contentItem: Item
    {
        anchors.left: parent.left

        IconImage
        {
            id: icon

            width: 20
            height: 20

            opacity: enabled ? 1.0 : 0.3
            visible: !!source

            source: control.iconSource
            color: control.currentColor
        }

        Text
        {
            id: layoutNameLabel

            x: icon.visible ? icon.width + 4 : 0
            anchors.verticalCenter: icon.verticalCenter

            verticalAlignment: Text.AlignVCenter
            elide: Qt.ElideRight
            font: control.font
            text: control.text
            opacity: enabled ? 1.0 : 0.3

            color: control.currentColor
        }

        Text
        {
            id: logicalIdLabel

            anchors.left: layoutNameLabel.right
            anchors.right: parent.right
            height: parent.height

            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: ColorTheme.lighter(ColorTheme.windowText, 4)

            visible: control.logicalId != 0

            // `u2013` is a long dash.
            text: " \u2013 " + qsTr("Logical ID") + " " + control.logicalId
        }
    }
}
