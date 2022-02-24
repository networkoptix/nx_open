// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// This file is needed only while QML ResourceBrowser is displayed over the C++ scene.

import QtQuick 2.15

import Nx.Items 1.0

Item
{
    readonly property alias resourceBrowser: d

    ResourceBrowser
    {
        id: d
        anchors.fill: parent
    }
}
