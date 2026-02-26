// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as QuickControls

import Nx.Controls
import Nx.Core

import nx.vms.client.mobile

QuickControls.Page
{
    default property alias data: contentColumn.data

    property Item rightControl

    function saveSettings()
    {
        settingsSaved()
    }

    signal settingsSaved
    signal error

    padding: 20

    background: Rectangle
    {
        color: ColorTheme.colors.dark4
    }

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: width
        contentHeight: contentColumn.height

        Column
        {
            id: contentColumn

            width: flickable.width
            spacing: 4
        }
    }
}
