// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import Nx.Core 1.0
import Nx.Core.Controls

Item
{
    id: item

    property int checkState: Qt.Unchecked

    property bool hovered: false
    property bool pressed: false
    property bool focused: false
    property bool enabled: true

    property color fillColorOff : "transparent"
    property color fillColorOn :
    {
        if (hovered || focused)
            return ColorTheme.colors.green_l
        if (pressed)
            return ColorTheme.colors.green_d
        return ColorTheme.colors.green
    }
    property color frameColorOff:
    {
        if (hovered)
            return ColorTheme.colors.light8
        if (checkState === Qt.PartiallyChecked)
            return ColorTheme.colors.light4
        if (focused)
            return ColorTheme.colors.light6
        if (pressed)
            return ColorTheme.colors.light12
        return ColorTheme.colors.light10
    }
    property color frameColorOn:
    {
        if (focused)
            return ColorTheme.colors.light2
        if (checkState === Qt.PartiallyChecked)
            return frameColorOff
        return "transparent"
    }

    property color gripColor:
    {
        return checkState === Qt.Checked ? ColorTheme.colors.dark5 : frameColorOff
    }

    property int animationDuration: 0

    implicitWidth: 33
    implicitHeight: 17
    baselineOffset: implicitHeight - 2
    opacity: enabled ? 1 : 0.5

    ColoredImage
    {
        id: checkedIcon

        visible: checkState === Qt.Checked
        anchors.fill: parent
        primaryColor: item.fillColorOn
        secondaryColor: item.frameColorOn
        tertiaryColor: item.gripColor
        sourcePath: "image://skin/33x17/Solid/switch_on.svg"
        sourceSize: Qt.size(item.implicitWidth, item.implicitHeight)
    }

    ColoredImage
    {
        id: uncheckedIcon

        anchors.fill: parent
        visible: checkState === Qt.Unchecked
        primaryColor: item.frameColorOff
        secondaryColor: item.gripColor
        sourcePath: "image://skin/33x17/Outline/switch_off.svg"
        sourceSize: Qt.size(item.implicitWidth, item.implicitHeight)
    }

    ColoredImage
    {
        id: partiallyCheckedIcon

        visible: checkState === Qt.PartiallyChecked
        anchors.fill: parent
        primaryColor: item.frameColorOff
        secondaryColor: item.gripColor
        sourcePath: "image://skin/33x17/Outline/switch_middle.svg"
        sourceSize: Qt.size(item.implicitWidth, item.implicitHeight)
    }
}
