// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Core.Items 1.0
import nx.vms.client.core 1.0

Rectangle
{
    id: control

    property alias resource: thumbnail.resource
    property alias timestampMs: thumbnail.timestampMs

    color: ColorTheme.colors.dark3

    Image
    {
        id: image

        anchors.fill: parent
        fillMode: Qt.KeepAspectRatio
        source: thumbnail.url
        visible: image.frameCount > 0
    }

    ThreeDotBusyIndicator
    {
        anchors.centerIn: parent
        visible: !image.visible
    }

    ResourceIdentificationThumbnail
    {
        id: thumbnail
        maximumSize: width
    }
}
