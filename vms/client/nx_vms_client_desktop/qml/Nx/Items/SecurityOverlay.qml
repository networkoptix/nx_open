// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

Rectangle
{
    id: overlay

    enum Mode { Live, Archive }
    property int mode: SecurityOverlay.Live

    property alias resource: accessHelper.resource
    property alias resourceId: accessHelper.resourceId

    property alias font: textItem.font
    property alias textColor: textItem.color

    readonly property bool active:
    {
        if (!resource)
            return false

        if (ScreenRecording.isOn && !accessHelper.canExportArchive)
            return true

        const hasPermissions = mode == SecurityOverlay.Live
            ? accessHelper.canViewLive
            : accessHelper.canViewArchive

        return !hasPermissions
    }

    color: ColorTheme.colors.dark4
    visible: active

    AccessHelper { id: accessHelper }

    Text
    {
        id: textItem

        anchors.centerIn: overlay
        color: ColorTheme.colors.red_d1
        font.capitalization: Font.AllUppercase
        font.pixelSize: 24
        text: qsTr("No access")
    }
}
