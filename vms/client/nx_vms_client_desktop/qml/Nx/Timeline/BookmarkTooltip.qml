// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls

import nx.vms.client.core

import "private"

Page
{
    id: root

    property var self
    property var bookmarks: []
    property bool allowExport: true
    property bool readOnly: false

    readonly property int kMaxHeight: 300

    padding: 0
    background: Rectangle
    {
        color: "#042A49"
        opacity: 0.8
        radius: 2
    }

    ScrollView
    {
        id: scrollView
        implicitWidth: bookmarksStackLayout.width
        implicitHeight:
        {
            const availableHeight = root.kMaxHeight - root.footer.implicitHeight

            if (bookmarksStackLayout.height > availableHeight)
                return availableHeight

            return bookmarksStackLayout.height
        }

        ScrollBar.vertical: ScrollBar
        {
            parent: scrollView
            x: scrollView.width - width
            y: scrollView.topPadding
            height: scrollView.availableHeight
            active: scrollView.ScrollBar.horizontal.active

            contentItem: Rectangle
            {
                implicitWidth: 8
                color: "#349EF4"
            }

            background: Rectangle
            {
                color: "#042A49"
            }
        }

        StackLayout
        {
            id: bookmarksStackLayout
            currentIndex: pageController.current - 1

            Repeater
            {
                model: root.bookmarks

                BookmarkView
                {
                    readonly property int kMaxWidth: 300
                    readonly property int kMinWidth: 180
                    property bool isCurrent: StackLayout.isCurrentItem
                    onIsCurrentChanged:
                    {
                        StackLayout.layout.implicitHeight = implicitHeight
                    }

                    Layout.fillHeight: false
                    Layout.fillWidth: false
                    Layout.preferredWidth:
                    {
                        if (implicitWidth < kMinWidth)
                            return kMinWidth

                        if (implicitWidth > kMaxWidth)
                            return kMaxWidth

                        return implicitWidth
                    }

                    bookmark: modelData
                    bottomPadding: 0
                    clip: true

                    onTagClicked: function(tag)
                    {
                        root.self.tagClicked(tag)
                    }
                }
            }
        }
    }

    footer: Control
    {
        padding: 16
        contentItem: ColumnLayout
        {
            spacing: 0

            RowLayout
            {
                id: bookmarkControls

                Layout.fillWidth: true

                spacing: 8

                ImageButton
                {
                    icon.source: "image://skin/20x20/Solid/play.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: "white"

                    onClicked: root.self.onPlayClicked(bookmarksStackLayout.currentIndex)
                }

                ImageButton
                {
                    icon.source: "image://skin/20x20/Solid/edit.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: "white"
                    visible: !root.readOnly

                    onClicked: root.self.onEditClicked(bookmarksStackLayout.currentIndex)
                }

                ImageButton
                {
                    icon.source: "image://skin/20x20/Solid/download.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: "white"
                    visible: root.allowExport

                    onClicked: root.self.onExportClicked(bookmarksStackLayout.currentIndex)
                }

                Item
                {
                    Layout.fillWidth: true
                }

                ImageButton
                {
                    icon.source: "image://skin/20x20/Solid/delete.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: "white"
                    visible: !root.readOnly

                    onClicked: root.self.onDeleteClicked(bookmarksStackLayout.currentIndex)
                }
            }

            Item
            {
                Layout.fillWidth: true

                implicitHeight: 32
                visible: root.bookmarks.length > 1

                Rectangle
                {
                    width: parent.width
                    anchors.verticalCenter: parent.verticalCenter
                    height: 1
                    color: "#349EF4"
                    opacity: 0.5
                }
            }

            PageController
            {
                id: pageController

                Layout.fillWidth: true

                visible: root.bookmarks.length > 1
                padding: 0
                total: root.bookmarks.length
            }
        }
    }
}
