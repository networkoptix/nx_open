// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// This file is needed only while QML LeftPanel is displayed over the C++ scene.

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

import ".."

Item
{
    id: wrapper

    readonly property alias panel: panel
    readonly property alias openCloseButton: openCloseButton

    width: borderControls.x + borderControls.width

    LeftPanel
    {
        id: panel
        height: wrapper.height
    }

    Row
    {
        id: borderControls

        x: panel.width - panel.resizerPadding
        height: wrapper.height
        z: 1

        Rectangle
        {
            width: 1
            height: borderControls.height
            color: ColorTheme.colors.dark8
        }

        DeprecatedIconButton
        {
            id: openCloseButton

            y: (borderControls.height - openCloseButton.height) / 2
            width: 12
            height: 32

            checkable: true

            iconBaseName: "panel/slide_right"
            iconCheckedBaseName: "panel/slide_left"
        }
    }
}
