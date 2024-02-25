// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

AbstractButton
{
    id: iconButton

    property string iconDir: "qrc:/skin"
    property string iconExtension: "png"
    property string iconBaseName: ""
    property string iconCheckedBaseName: ""

    property string iconNormalName: iconBaseName
    property string iconPressedName: iconBaseName + "_pressed"
    property string iconHoveredName: iconBaseName + "_hovered"
    property string iconDisabledName: iconBaseName + "_disabled"

    property string iconCheckedNormalName: iconCheckedBaseName
    property string iconCheckedPressedName: iconCheckedBaseName + "_pressed"
    property string iconCheckedHoveredName: iconCheckedBaseName + "_hovered"
    property string iconCheckedDisabledName: iconCheckedBaseName + "_disabled"

    readonly property string currentIconName:
    {
        if (checked)
        {
            if (!enabled)
                return iconCheckedDisabledName
            if (pressed)
                return iconCheckedPressedName
            return hovered ? iconCheckedHoveredName : iconCheckedNormalName
        }
        else
        {
            if (!enabled)
                return iconDisabledName
            if (pressed)
                return iconPressedName
            return hovered ? iconHoveredName : iconNormalName
        }
    }

    readonly property string currentIconPath: "%1/%2.%3"
        .arg(iconDir)
        .arg(currentIconName)
        .arg(iconExtension)

    background: Image { source: iconButton.currentIconPath }

    focusPolicy: Qt.TabFocus
}
