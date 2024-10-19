// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4
import Qt5Compat.GraphicalEffects

import Nx.Controls 1.0
import Nx.Core.Items 1.0
import Nx.Core.Controls 1.0

Popup
{
    id: control

    property alias blurSource: blurEffect.source

    closePolicy: Popup.NoAutoClose
    focus: true

    width: mainWindow.width
    height: mainWindow.height

    background: GaussianBlur
    {
        id: blurEffect

        deviation: 2
    }


    contentItem: Item
    {
        NxDotPreloader
        {
            anchors.centerIn: parent
        }
    }

    onVisibleChanged:
    {
        if (visible)
            content.forceActiveFocus()
    }
}
