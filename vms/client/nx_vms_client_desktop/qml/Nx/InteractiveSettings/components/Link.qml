// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

import nx.vms.client.desktop 1.0

import "private"

/**
 * Interactive Settings type.
 */
LabeledItem
{
    property string url

    contentItem: Text
    {
        text: `<a href="${url}">${url}</a>`
        textFormat: Text.StyledText
        linkColor: mouseArea.containsMouse
            ? ColorTheme.colors.brand_core
            : ColorTheme.colors.brand_d2

        onLinkActivated: Qt.openUrlExternally(url)

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent
            cursorShape: containsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor
            acceptedButtons: Qt.NoButton
            hoverEnabled: true
            CursorOverride.shape: cursorShape
            CursorOverride.active: containsMouse
        }
    }
}
