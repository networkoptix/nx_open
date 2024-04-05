// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

Control
{
    id: control

    property string iconSource: ""
    property alias text: text.text
    property alias checked: checkBox.checked
    property bool current: false

    signal clicked()

    implicitHeight: 28
    baselineOffset: text.baselineOffset

    leftPadding: iconSource ? 6 : 8
    rightPadding: 8

    background: Rectangle
    {
        color: mouseArea.containsMouse || control.current
            ? ColorTheme.colors.dark15
            : ColorTheme.colors.dark13
    }

    contentItem: RowLayout
    {
        id: layout

        spacing: 0

        baselineOffset: text.baselineOffset

        SvgImage
        {
            id: icon

            Layout.alignment: Qt.AlignVCenter

            sourcePath: iconSource
            primaryColor: "light10"
            secondaryColor: "light4"
            visible: !!iconSource
            sourceSize: Qt.size(20, 20)
        }

        Text
        {
            id: text

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            elide: Text.ElideRight
            color: ColorTheme.colors.light4
            font: Qt.font({pixelSize: 14, weight: Font.Normal})

            textFormat: Text.StyledText
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

        onClicked: control.clicked()
    }
}
