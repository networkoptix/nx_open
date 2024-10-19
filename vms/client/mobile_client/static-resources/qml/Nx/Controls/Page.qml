// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Controls 1.0

PageBase
{
    id: control

    property alias leftButtonIcon: toolBar.leftButtonIcon
    property alias titleControls: toolBar.controls
    property alias warningText: warningPanel.text
    property alias warningVisible: warningPanel.opened
    property alias toolBar: toolBar
    property alias warningPanel: warningPanel
    property alias titleLabelOpacity: toolBar.titleOpacity

    signal leftButtonClicked()
    signal headerClicked()

    clip: true

    header: Item
    {
        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        Column
        {
            id: column
            width: parent.width
            topPadding: toolBar.statusBarHeight

            ToolBar
            {
                id: toolBar
                title: control && control.title
                leftButtonIcon.source: lp("/images/arrow_back.png")
                onLeftButtonClicked: control.leftButtonClicked()
                onClicked: control.headerClicked()
            }

            WarningPanel
            {
                id: warningPanel
            }
        }
    }
}
