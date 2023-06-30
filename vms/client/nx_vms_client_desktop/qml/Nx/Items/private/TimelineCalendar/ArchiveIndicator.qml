// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Core 1.0

Item
{
    property bool cameraHasArchive: false
    property bool anyCameraHasArchive: false

    anchors
    {
        bottom: parent ? parent.bottom : undefined
        bottomMargin: 6
        horizontalCenter: parent ? parent.horizontalCenter : undefined
    }

    width: 16
    height: 2

    Rectangle
    {
        anchors.fill: parent
        color: cameraHasArchive ? ColorTheme.colors.green_l1 : ColorTheme.colors.green_d1
        radius: height / 2
        visible: cameraHasArchive || anyCameraHasArchive
    }
}
