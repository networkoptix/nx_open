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

    width: bookmarks.length > 1 ? 300 : undefined
    height: bookmarks.length > 1 ? 300 : undefined

    padding: 0
    background: Rectangle
    {
        color: ColorTheme.colors.timeline.bookmark.tooltip.background
        opacity: 0.8
        radius: 2
    }

    contentItem: Item
    {
        implicitWidth: Math.min(bookmarkLoader.implicitWidth, 300 - footer.implicitWidth)
        implicitHeight: Math.min(bookmarkLoader.implicitHeight, 300 - footer.implicitHeight)

        Loader
        {
            id: bookmarkLoader
            active: false
            anchors.fill: parent

            sourceComponent: Component
            {
                BookmarkView
                {
                    id: bookmarkView
                    width: bookmarkLoader.width
                    height: bookmarkLoader.height
                    bookmark: root.bookmarks[pageController.current - 1]

                    onTagClicked: function(tag)
                    {
                        root.self.tagClicked(tag)
                    }
                }
            }

            Connections
            {
                target: pageController
                function onCurrentChanged()
                {
                    bookmarkLoader.active = false;
                    if (bookmarks.length > 0)
                        bookmarkLoader.active = true;
                }
            }
        }
    }

    footer: Control
    {
        id: footer
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
                    implicitWidth: 30
                    implicitHeight: 30
                    icon.source: "image://skin/20x20/Solid/play.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: ColorTheme.colors.light4

                    onClicked: root.self.onPlayClicked(pageController.current - 1)
                }

                ImageButton
                {
                    implicitWidth: 30
                    implicitHeight: 30
                    icon.source: "image://skin/20x20/Solid/edit.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: ColorTheme.colors.light4
                    visible: !root.readOnly

                    onClicked: root.self.onEditClicked(pageController.current - 1)
                }

                ImageButton
                {
                    implicitWidth: 30
                    implicitHeight: 30
                    icon.source: "image://skin/20x20/Solid/download.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: ColorTheme.colors.light4
                    visible: root.allowExport

                    onClicked: root.self.onExportClicked(pageController.current - 1)
                }

                Item
                {
                    Layout.fillWidth: true
                }

                ImageButton
                {
                    implicitWidth: 30
                    implicitHeight: 30
                    icon.source: "image://skin/20x20/Solid/delete.svg"
                    icon.width: 20
                    icon.height: 20
                    icon.color: ColorTheme.colors.light4
                    visible: !root.readOnly

                    onClicked: root.self.onDeleteClicked(pageController.current - 1)
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
