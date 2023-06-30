// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

TabControl
{
    id: tabControl

    property real dialogLeftPadding: 0
    property real dialogRightPadding: 0

    tabBar.spacing: 20

    tabBar.background: Item
    {
        Rectangle
        {
            color: ColorTheme.colors.dark10
            x: - tabControl.dialogLeftPadding
            width: tabControl.dialogLeftPadding + parent.width + tabControl.dialogRightPadding
            height: parent.height

            Rectangle
            {
                color: ColorTheme.colors.dark6
                width: parent.width
                height: 1
                anchors.bottom: parent.bottom
            }
        }
    }

    component TabButton: CompactTabButton
    {
        height: tabControl.tabBar.height

        underlineOffset: -1
        topPadding: 14
        focusFrameEnabled: false

        compact: false

        underlineHeight: 2
        inactiveUnderlineColor: "transparent"
        font: Qt.font({pixelSize: 12, weight: Font.Normal})
    }

    function switchTab(tabIndex, callbackAfterSwitching)
    {
        if (dialog.tabIndex === tabIndex)
        {
            callbackAfterSwitching()
        }
        else
        {
            function tabSwitchHandler()
            {
                callbackAfterSwitching()
                tabControl.tabSwitched.disconnect(tabSwitchHandler)
            }
            tabControl.tabSwitched.connect(tabSwitchHandler)
            dialog.tabIndex = tabIndex
        }
    }
}
