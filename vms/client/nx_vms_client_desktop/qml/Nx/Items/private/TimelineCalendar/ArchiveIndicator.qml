// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx 1.0

Rectangle
{
    property bool cameraHasArchive: false
    property bool anyCameraHasArchive: false

    anchors
    {
        bottom: parent ? parent.bottom : undefined
        bottomMargin: 5
        horizontalCenter: parent ? parent.horizontalCenter : undefined
    }

    color: ColorTheme.colors.green_l4
    radius: height / 2

    width: cameraHasArchive ? 12 : 8
    height: cameraHasArchive ? 2 : 1
    opacity: cameraHasArchive ? 1.0 : 0.5
    visible: cameraHasArchive || anyCameraHasArchive
}
