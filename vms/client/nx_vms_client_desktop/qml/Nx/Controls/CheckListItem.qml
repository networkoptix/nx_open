// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

import nx.vms.client.core

import "private"

Control
{
    id: control

    property string iconSource: ""
    property alias text: text.text
    property alias checked: checkBox.checked
    property ButtonColors primaryColors: ButtonColors { normal: ColorTheme.colors.light10 }
    property ButtonColors secondaryColors: ButtonColors { normal: ColorTheme.colors.light4 }
    property ButtonColors checkedPrimaryColors: ButtonColors { normal: ColorTheme.colors.light4 }
    property ButtonColors checkedSecondaryColors: ButtonColors { normal: ColorTheme.colors.light4 }

    signal clicked()

    implicitHeight: 20
    baselineOffset: text.baselineOffset
    spacing: 4

    function color(buttonColors, checkedButtonColors)
    {
        let colors = checked ? checkedButtonColors : buttonColors
        return hovered ? colors.hovered : colors.normal
    }

    contentItem: RowLayout
    {
        id: layout

        spacing: control.spacing
        baselineOffset: text.baselineOffset

        ColoredImage
        {
            id: icon

            Layout.alignment: Qt.AlignVCenter

            primaryColor: control.color(control.primaryColors, control.checkedPrimaryColors)
            secondaryColor: control.color(control.secondaryColors, control.checkedSecondaryColors)
            opacity: enabled ? 1.0 : 0.3
            visible: !!control.iconSource
            sourcePath: control.iconSource
            sourceSize: Qt.size(20, 20)
        }

        Text
        {
            id: text

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            elide: Text.ElideRight
            color: control.color(control.primaryColors, control.checkedPrimaryColors)
            opacity: enabled ? 1.0 : 0.3
            textFormat: Text.StyledText
            font.pixelSize: FontConfig.normal.pixelSize
        }

        CheckBox
        {
            id: checkBox

            Layout.alignment: Qt.AlignBaseline
            Layout.bottomMargin: 1 //< Position adjustment.
            focusPolicy: Qt.NoFocus
        }
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true

        onClicked:
        {
            checked = !checked
            control.clicked()
        }
    }
}
