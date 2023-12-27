// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Debug

import nx.vms.client.desktop

Item
{
    id: rootItem

    property bool performanceInfoVisible: false
    property real notificationsPanelX: 0

    RhiRenderingItem
    {
        objectName: "renderingItem"
    }

    Text
    {
        visible: LocalSettings.iniConfigValue("developerMode")

        anchors
        {
            top: parent.top
            right: performanceText.left
            topMargin: 16
            rightMargin: 16
        }
        color: "#505050"
        text: "Experimental: %1".arg(LocalSettings.iniConfigValue("graphicsApi"))
        font.pixelSize: 20
    }

    PerformanceText
    {
        id: performanceText
        visible: rootItem.performanceInfoVisible

        anchors.top: parent.top
        x: rootItem.notificationsPanelX - width
    }
}
