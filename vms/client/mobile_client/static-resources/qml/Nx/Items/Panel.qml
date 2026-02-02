// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls as NxControls
import Nx.Core
import Nx.Items
import Nx.Mobile.Ui
import Nx.Ui

Page
{
    id: root

    property alias item: proxyItem.target
    property bool interactive: true
    property color color: ColorTheme.colors.dark4
    property string iconSource
    property alias availableHeaderArea: availableHeaderArea

    signal closeButtonClicked

    implicitWidth: 330
    background: Rectangle { color: root.color }
    padding: 0

    header: Item
    {
        implicitHeight: 56
        implicitWidth: root.width

        Rectangle
        {
            anchors.fill: parent
            color: root.color
        }

        RowLayout
        {
            anchors.fill: parent

            TitleLabel
            {
                Layout.leftMargin: 20
                horizontalAlignment: Text.AlignLeft
                text: root.title
            }

            Item
            {
                id: availableHeaderArea

                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            NxControls.IconButton
            {
                padding: 0
                icon.source: "image://skin/24x24/Outline/close.svg?primary=light4"
                icon.width: 24
                icon.height: 24
                visible: root.interactive
                onClicked: root.closeButtonClicked()
            }
        }

        Rectangle
        {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: ColorTheme.colors.dark7
            visible: !LayoutController.isTabletLayout
        }
    }

    contentItem: ProxyItem
    {
        id: proxyItem
    }
}
