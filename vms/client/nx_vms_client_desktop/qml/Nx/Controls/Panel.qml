// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Nx.Core
import nx.vms.client.core
import nx.vms.client.desktop

GroupBox
{
    id: control

    leftPadding: 0
    rightPadding: 0
    topPadding: 32
    bottomPadding: 20

    font.pixelSize: FontConfig.large.pixelSize

    property alias checkable: switchItem.visible
    property alias checked: switchItem.checked

    signal triggered(bool checked)

    background: Rectangle
    {
        y: 19
        width: parent.availableWidth
        height: 1
        color: ColorTheme.colors.dark12
    }

    label: RowLayout
    {
        Layout.alignment: Qt.AlignRight
        spacing: 8
        width: control.width

        SwitchButton
        {
            id: switchItem

            visible: false

            spacing: 0
            topPadding: 0
            bottomPadding: 3
            leftPadding: 0
            rightPadding: 0
            background: null
            implicitHeight: 15

            onClicked: control.triggered(checked)
        }

        Text
        {
            Layout.fillWidth: true
            text: control.title
            font: control.font
            elide: Text.ElideRight
            color: ColorTheme.colors.light4

            MouseArea
            {
                anchors.fill: parent
                onClicked: checked = !checked
            }
        }
    }
}
