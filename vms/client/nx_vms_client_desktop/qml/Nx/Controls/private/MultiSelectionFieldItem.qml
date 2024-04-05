// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

Control
{
    id: control

    property string text: ""
    property string color: ColorTheme.colors.light4
    property color backgroundColor: ColorTheme.colors.dark10
    property string iconSource: ""
    property bool removable: true

    signal removeClicked()

    background: Rectangle
    {
        color: backgroundColor
        radius: 2

        border.width: 1
        border.color: ColorTheme.colors.dark12
    }

    leftPadding: iconSource ? 4 : 8
    rightPadding: removable ? 2 : 8

    contentItem: RowLayout
    {
        spacing: 0

        baselineOffset: text.baselineOffset

        SvgImage
        {
            Layout.alignment: Qt.AlignVCenter

            visible: !!iconSource
            sourcePath: iconSource
            primaryColor: "light10"
            secondaryColor: "light4"
            sourceSize: Qt.size(20, 20)
        }

        Text
        {
            id: text

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 22

            text: control.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: control.color
            font: Qt.font({pixelSize: 14, weight: Font.Normal})
        }

        ImageButton
        {
            Layout.alignment: Qt.AlignVCenter

            visible: removable

            icon.source: "image://svg/skin/user_settings/cross_input.svg"
            icon.width: 20
            icon.height: 20
            icon.color: ColorTheme.colors.light10

            onClicked: control.removeClicked()
        }
    }
}
