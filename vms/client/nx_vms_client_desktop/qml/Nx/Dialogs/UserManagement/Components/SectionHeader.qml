// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0

Item
{
    id: control
    height: 44
    width: parent ? parent.width : headerText.width

    property alias text: headerText.text
    property string additionalText: ""
    property int counter: 0

    baselineOffset: headerText.y + headerText.baselineOffset

    Text
    {
        id: headerText

        height: 20
        font: Qt.font({pixelSize: 16, weight: Font.Medium})
        color: ColorTheme.colors.light4

        anchors.top: parent.top
        anchors.topMargin: 16
    }

    Text
    {
        id: additionaText
        font: Qt.font({pixelSize: 14, weight: Font.Medium})
        color: ColorTheme.colors.dark13
        text: control.additionalText ? "(%1)".arg(control.additionalText) : ""

        anchors.baseline: headerText.baseline
        anchors.left: headerText.right
        anchors.leftMargin: 4
    }

    Rectangle
    {
        height: 1
        width: parent.width

        color: ColorTheme.colors.dark12

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
    }
}
