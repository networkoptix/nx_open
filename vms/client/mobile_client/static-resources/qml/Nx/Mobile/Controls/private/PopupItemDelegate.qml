// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

import Nx.Core

T.ItemDelegate
{
    id: delegate

    property bool selected: false
    property alias textColor: textItem.color

    implicitWidth: parent?.width ?? 0
    implicitHeight: 48

    leftPadding: 16
    rightPadding: 16

    font.pixelSize: 16
    font.weight: 400

    background: Rectangle
    {
        color: delegate.selected
            ? ColorTheme.colors.brand_core
            : ColorTheme.colors.dark10
    }

    contentItem: Text
    {
        id: textItem

        text: delegate.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        elide: Text.ElideRight
        font: delegate.font
        color: delegate.selected
            ? ColorTheme.colors.dark1
            : ColorTheme.colors.light4
    }
}

