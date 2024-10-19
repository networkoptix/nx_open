// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx.Core 1.0

AbstractButton
{
    id: control

    font.pixelSize: 16
    font.weight: Font.Medium

    implicitHeight: 32
    implicitWidth: textItem.implicitWidth + control.leftPadding + control.rightPadding
    leftPadding: 8
    rightPadding: 8

    contentItem: Text
    {
        id: textItem

        text: control.text
        font: control.font
        color: ColorTheme.colors.brand_core

        verticalAlignment: Text.AlignVCenter
    }
}
