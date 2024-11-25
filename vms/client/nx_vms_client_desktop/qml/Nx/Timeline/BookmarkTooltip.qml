// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Core

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
        color: ColorTheme.colors.timeline.bookmark.tooltip.background
        opacity: 0.8
        radius: 2
    }

    function updateHeight()
    {
        height = footer.height + bookmarksStackLayout.children[bookmarksStackLayout.currentIndex].height
    }

    StackLayout
    {
        id: bookmarksStackLayout
        currentIndex: pageController.current - 1
        clip: true

        Repeater
        {
            model: root.bookmarks

            BookmarkView
            {
                property bool isCurrent: StackLayout.isCurrentItem

                Layout.fillWidth: true
                Layout.maximumWidth: 300
                Layout.minimumWidth: 180

                Layout.fillHeight: false
                Layout.maximumHeight: 250

                bookmark: modelData

                function updateRootHeight()
                {
                    // Bookmark tooltip height must be equal to the current bookmark content height
                    // plus root item footer height. The given workaround is required as height
                    // is recalculated multiple time during layout population and changing
                    // current item.
                    if (isCurrent)
                        Qt.callLater(root.updateHeight)
                }

                onIsCurrentChanged: updateRootHeight()
                onHeightChanged: updateRootHeight()

                onTagClicked: function(tag)
                {
                    root.self.tagClicked(tag)
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
                    icon.color: ColorTheme.colors.light4

                    onClicked: root.self.onPlayClicked(bookmarksStackLayout.currentIndex)
                }

                ImageButton
                {
                    icon.source: "image://skin/20x20/Solid/edit.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: ColorTheme.colors.light4
                    visible: !root.readOnly

                    onClicked: root.self.onEditClicked(bookmarksStackLayout.currentIndex)
                }

                ImageButton
                {
                    icon.source: "image://skin/20x20/Solid/download.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: ColorTheme.colors.light4
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
                    icon.color: ColorTheme.colors.light4
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
                    color: ColorTheme.colors.timeline.bookmark.tooltip.separator
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
