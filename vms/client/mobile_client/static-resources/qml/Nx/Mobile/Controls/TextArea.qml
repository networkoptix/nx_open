// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

ScrollView
{
    id: control

    property alias backgroundMode: fieldBackground.mode
    property alias labelText: fieldBackground.labelText
    property alias text: textArea.text
    property alias readOnly: textArea.readOnly

    implicitWidth: 268
    implicitHeight: Math.min(150, textArea.implicitHeight)

    TextArea
    {
        id: textArea

        wrapMode: TextArea.Wrap
        width: parent.width

        topPadding: 30
        leftPadding: 12
        rightPadding: 12

        font.pixelSize: 16
        font.weight: 500
        color: enabled
            ? ColorTheme.colors.light4
            : ColorTheme.transparent(ColorTheme.colors.light4, 0.3)

        background: FieldBackground
        {
            id: fieldBackground

            owner: textArea
        }

        focus: false
    }
}
