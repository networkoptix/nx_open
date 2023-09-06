// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls

Control
{
    id: tabControl

    property alias tabBar: tabBar
    property alias currentTabIndex: tabBar.currentIndex

    spacing: 0 //< Spacing between tabbar and content.

    default property list<Tab> tabs

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

    readonly property alias currentTab: tabPages.currentTab
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

                    var itemsCount = count
                    for (let i = 0; i < itemsCount; ++i)
                        takeItem(0)

                    for (let i = 0; i < buttons.length; ++i)
                        addItem(buttons[i])

                    currentIndex = Math.max(buttons.indexOf(previousTab), 0)
                }

                onCurrentIndexChanged:
                    tabPages.currentTab = tabControl.visibleTabs[currentIndex] || null
            }
        }

        Item
        {
            id: tabPages

            Layout.fillWidth: true
            Layout.fillHeight: true

            implicitWidth: implicitSize.width
            implicitHeight: implicitSize.height

            property Tab currentTab: tabControl.visibleTabs[0] || null

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
                model: tabControl.tabs
                anchors.fill: parent

                Fader
                {
                    id: fader

                    readonly property Tab tab: tabControl.tabs[index]

                    anchors.fill: parent
                    opaque: tab.visible && tab === tabPages.currentTab
                    contentItem: tab.page

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
