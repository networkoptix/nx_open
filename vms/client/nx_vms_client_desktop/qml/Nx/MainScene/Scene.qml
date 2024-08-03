// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Debug

import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: rootItem

    property bool performanceInfoVisible: false
    property real notificationsPanelX: 0
    property real titleY: 0

    RhiRenderingItem
    {
        objectName: "renderingItem"
    }

    PerformanceText
    {
        id: performanceText
        visible: rootItem.performanceInfoVisible

        x: rootItem.notificationsPanelX - width
        y: rootItem.titleY
    }

    TextureSizeHelper
    {
        objectName: "textureSizeHelper"
    }
}
