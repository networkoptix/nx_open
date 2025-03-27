// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

import Nx.Core

T.ItemDelegate
{
    id: delegate

    property bool selected: false
    property string textRole

    implicitWidth: parent?.width ?? 0
    implicitHeight: 56

    leftPadding: 16
    rightPadding: 16

    background: Rectangle
    {
        color: delegate.selected
            ? ColorTheme.colors.brand_core
            : ColorTheme.colors.dark7
    }

    contentItem: Text
    {
        verticalAlignment: Text.AlignVCenter

        elide: Text.ElideRight
        font.pixelSize: 16
        font.weight: 400
        color: delegate.selected
            ? ColorTheme.colors.light1
            : ColorTheme.colors.light4

        text: delegate.textRole
            ? modelData[delegate.textRole]
            : modelData
    }
}

