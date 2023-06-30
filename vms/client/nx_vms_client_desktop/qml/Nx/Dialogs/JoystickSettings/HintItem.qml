// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

Rectangle
{
    id: hint

    readonly property string labelText: qsTr("Hold %1 to activate actions",
        "Leave %1 as is. It will be replaced to button name.")
    readonly property color hintColor: ColorTheme.colors.brand_core

    property alias buttonName: label.text

    height: 50

    color: "transparent"
    border.color: hintColor
    border.width: 1
    radius: 2

    Row
    {
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft

        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter

        spacing: 5

        Label
        {
            id: startTextLabel

            anchors.verticalCenter: buttonLabelRect.verticalCenter

            color: hintColor
            font.pixelSize: FontConfig.normal.pixelSize

            text: hint.labelText.substring(0, hint.labelText.indexOf("%1")).trim()
        }

        Rectangle
        {
            id: buttonLabelRect

            width: label.width + 8
            height: label.height + 4

            radius: 2

            color: hintColor

            Label
            {
                id: label

                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter

                font.bold: true
                font.pixelSize: 10
                font.capitalization: Font.AllUppercase
                color: ColorTheme.colors.dark5
            }
        }

        Label
        {
            anchors.verticalCenter: buttonLabelRect.verticalCenter

            color: startTextLabel.color
            font.pixelSize: startTextLabel.font.pixelSize

            text: hint.labelText.substring(hint.labelText.indexOf("%1") + 3).trim()
        }
    }
}
