// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls as Nx

import "private"

/**
 * Interactive Settings type.
 */
LabeledItem
{
    id: control

    property string text: ""

    contentItem: Nx.Label
    {
        text: control.text

        wrapMode: Text.Wrap
        textFormat: Text.StyledText
        linkColor: hoveredLink
            ? ColorTheme.colors.brand_core
            : ColorTheme.colors.brand_d2

        onLinkActivated: (link) => Qt.openUrlExternally(link)
    }
}
