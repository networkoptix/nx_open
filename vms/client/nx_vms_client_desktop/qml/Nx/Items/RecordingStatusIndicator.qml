// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.desktop

Item
{
    property alias resource: statusHelper.resource

    implicitWidth: image.implicitWidth
    implicitHeight: image.implicitHeight

    RecordingStatusHelper
    {
        id: statusHelper
    }

    ColoredImage
    {
        id: image
        anchors.centerIn: parent
        sourceSize: Qt.size(20, 20)

        sourcePath: statusHelper.qmlIconName

        primaryColor: (extras.flags & ResourceTree.ResourceExtraStatusFlag.recording
            || extras.flags & ResourceTree.ResourceExtraStatusFlag.scheduled)
            ? "red_l"
            : "dark17"
    }
}
