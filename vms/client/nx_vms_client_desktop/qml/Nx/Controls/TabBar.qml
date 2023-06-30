// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

TabBar
{
    id: tabBar

    property color bottomBorderColor: ColorTheme.colors.dark10
    property color color: "transparent"

    readonly property var fullWidth: tabBar.contentWidth + tabBar.leftPadding + tabBar.rightPadding

    readonly property bool scrollerVisible: fullWidth > tabBar.width

    readonly property bool canScrollLeft:
        tabBar.contentItem.contentX > 0

    readonly property bool canScrollRight: // 2 is fine tuned for better UX.
        tabBar.contentItem.contentX < fullWidth + tabBar.scrollerWidth - tabBar.width - 2

    onScrollerVisibleChanged:
        ensureIndexIsVisible(tabBar.currentIndex)

    property int scrollerWidth: 50

    padding: 0
    spacing: 8

    height: implicitHeight

    onWidthChanged:
        ensureIndexIsVisible(tabBar.currentIndex)

    onCurrentIndexChanged:
        ensureIndexIsVisible(tabBar.currentIndex)

    function ensureIndexIsVisible(index)
    {
        if (!scrollerVisible)
            return

        if (index < 0)
            index = 0

        // Ensure an item is created for the index.
        tabBar.contentItem.positionViewAtIndex(index, ListView.Contain)

        const item = tabBar.contentItem.itemAtIndex(index)

        if (!item)
            return

        // Make sure the item is fully visible. We can not rely on positionViewAtIndex()
        // because the item should not be displayed under the scroller.
        if (item.x + item.width > contentItem.contentX + contentItem.width - scrollerWidth)
            contentItem.contentX = item.x - (contentItem.width - scrollerWidth) + item.width
        if (item.x < contentItem.contentX)
            contentItem.contentX = item.x
    }

    // Scroll to next tab after a tab at x, direction is either 1 or -1.
    function scrollToNext(x, direction)
    {
        const step = tabBar.spacing > 0 ? tabBar.spacing : 8

        // Search for next tab button.
        let index = tabBar.contentItem.indexAt(x + step * direction, tabBar.height / 2)

        // If we cannot get an index of the next tab button (either because we poked into spacing
        // or the item does not exit) then increment (decrement) index of the tab button at x.
        if (index == -1)
        {
            index = tabBar.contentItem.indexAt(x - step * direction, tabBar.height / 2)
            if (index == -1)
                return

            index += direction
        }

        // This can happen when direction == 1, just scroll to the last index.
        if (index >= tabBar.count)
            index = tabBar.count - 1

        ensureIndexIsVisible(index)
    }

    Component.onCompleted:
    {
        // contentItem is a ListView, set its properties to better serve tab bar scrolling.
        contentItem.footerPositioning = ListView.OverlayFooter
        contentItem.boundsBehavior = Flickable.StopAtBounds
        contentItem.snapMode = ListView.NoSnap

        // Scroller is implemented as an overlay over the ListView, so add a spacer at the end.
        contentItem.footer = scrollerSpacer
        tabBarOverlayComponent.createObject(contentItem, {})
    }

    Component
    {
        id: scrollerSpacer

        Item
        {
            width: tabBar.scrollerVisible ? tabBar.scrollerWidth : 0
            height: tabBar.height
            visible: tabBar.scrollerVisible
        }
    }

    Component
    {
        id: tabBarOverlayComponent

        Item
        {
            // The overlay should cover the whole TabBar.
            x: - tabBar.leftPadding
            y: - tabBar.topPadding

            width: tabBar.width
            height: tabBar.height

            visible: tabBar.scrollerVisible

            Rectangle
            {
                // A shadow on the left indicates that the TabBar can be scrolled there.
                id: leftShadow

                property color shadowColor: ColorTheme.colors.dark2

                visible: scrollLeftButton.enabled
                width: 8

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                gradient: Gradient
                {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: ColorTheme.transparent(leftShadow.shadowColor, 1.0) }
                    GradientStop { position: 1.0; color: ColorTheme.transparent(leftShadow.shadowColor, 0.0) }
                }
            }

            Rectangle
            {
                // This rectangle ensures that ListView items won't be visible when the scroll buttons
                // become transparent in disabled state.
                anchors.right: parent.right

                width: scroller.width
                height: tabBar.height

                color: tabBar.color

                MouseArea
                {
                    // Avoid tab selection via clicks though diabled buttons.
                    anchors.fill: parent
                }

                Row
                {
                    id: scroller
                    height: tabBar.height
                    width: tabBar.scrollerWidth
                    spacing: 0.5

                    ImageButton
                    {
                        id: scrollLeftButton

                        opacity: enabled ? 1.0 : 0.3
                        radius: 0
                        width: tabBar.scrollerWidth / 2 - 0.5
                        height: parent.height
                        icon.source: "image://svg/skin/tab_bar/arrow_left_24.svg"
                        icon.width: 24
                        icon.height: 24
                        enabled: tabBar.canScrollLeft
                        onClicked:
                            tabBar.scrollToNext(tabBar.contentItem.contentX - 1, -1)
                    }

                    Rectangle
                    {
                        // Scroll buttons separator.
                        width: 0.5
                        height: tabBar.height
                        color: ColorTheme.colors.dark6
                    }

                    ImageButton
                    {
                        id: scrollRightButton

                        opacity: enabled ? 1.0 : 0.3
                        radius: 0
                        width: tabBar.scrollerWidth / 2 - 0.5
                        height: parent.height
                        icon.source: "image://svg/skin/tab_bar/arrow_right_24.svg"
                        icon.width: 24
                        icon.height: 24
                        enabled: tabBar.canScrollRight
                        onClicked:
                            tabBar.scrollToNext(tabBar.contentItem.contentX + tabBar.width - tabBar.scrollerWidth + 1, 1)
                    }
                }

                Rectangle
                {
                    // A separator between tabs and the scroller.
                    anchors.right: scroller.left
                    width: 0.5
                    height: tabBar.height
                    color: ColorTheme.colors.dark12
                }
            }
        }
    }

    background: Rectangle
    {
        color: tabBar.color

        Rectangle
        {
            id: border

            color: tabBar.bottomBorderColor
            width: parent.width
            height: 1
            anchors.bottom: parent.bottom
        }
    }
}
