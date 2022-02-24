// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import Nx.Controls 1.0

Control
{
    id: tabControl

    property alias tabBar: tabBar

    spacing: 0 //< Spacing between tabbar and content.

    readonly property alias tabs: d.tabs

    readonly property var visibleTabs:
    {
        var result = []
        for (var i = 0; i < tabs.length; ++i)
        {
            var tab = tabs[i]
            if (tab.visible && tab.button)
                result.push(tab)
        }

        return result
    }

    enum SizePolicy
    {
        SizeToCurrentTab,
        SizeToAllTabs
    }

    property int sizePolicy: TabControl.SizeToAllTabs

    readonly property Tab currentTab: visibleTabs[tabBar.currentIndex] || null
    readonly property Item currentPage: currentTab && currentTab.page || null

    function selectTab(tab)
    {
        var index = visibleTabs.indexOf(tab)
        if (index !== -1)
            tabBar.currentIndex = index
    }

    function selectPage(page)
    {
        var index = visibleTabs.findIndex(function(tab) { return tab.page === page })
        if (index !== -1)
            tabBar.currentIndex = index
    }

    Component.onCompleted: d.tabs = Array.prototype.filter.call(data, item => item instanceof Tab)

    QtObject
    {
        id: d

        property var tabs: []
    }

    contentItem: ColumnLayout
    {
        spacing: tabControl.spacing

        Item
        {
            Layout.fillWidth: true
            implicitHeight: tabBar.height

            TabBar
            {
                id: tabBar

                x: 16
                width: parent.width - x * 2

                readonly property var buttons: tabControl.visibleTabs.map(
                    function(tab) { return tab.button })

                onButtonsChanged:
                {
                    var previousTab = currentItem

                    /* QML's TabBar has an internal bug. If some buttons were removed,
                     * then added again, and we clicked to not selected tab button, previous
                     * selected button disappears. It's properties "visible/opacity/geometry" not
                     * changes. It looks like it was removed from the scene graph.
                     */
                    contentChildren = buttons //< This duplication is a workaround
                    contentChildren = buttons //< for tab button disappearing.

                    currentIndex = Math.max(buttons.indexOf(previousTab), 0)
                }
            }
        }

        Item
        {
            id: tabPages

            Layout.fillWidth: true
            Layout.fillHeight: true

            implicitWidth: implicitSize.width
            implicitHeight: implicitSize.height

            readonly property var pages: tabControl.visibleTabs.map(
                function(tab) { return tab.page })

            readonly property size implicitSize:
            {
                if (tabControl.sizePolicy === TabControl.SizeToAllTabs)
                {
                    var reducer =
                        function(size, page)
                        {
                            if (!page)
                                return size

                            return Qt.size(
                                Math.max(size.width, page.implicitWidth),
                                Math.max(size.height, page.implicitHeight))
                        }

                    return pages.reduce(reducer, Qt.size(0, 0))
                }
                else
                {
                    var page = pages[tabBar.currentIndex]
                    return page ? Qt.size(page.implicitWidth, page.implicitHeight) : Qt.size(0, 0)
                }
            }

            Repeater
            {
                model: tabPages.pages.length
                anchors.fill: parent

                Fader
                {
                    id: fader

                    anchors.fill: parent
                    opaque: tabBar.currentIndex === index
                    contentItem: tabPages.pages[index]

                    Binding
                    {
                        target: contentItem
                        property: "parent"
                        value: fader
                    }
                }
            }
        }
    }
}
