// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

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

    Image
    {
        id: image
        anchors.centerIn: parent
        source: statusHelper.qmlIconName
    }
}
