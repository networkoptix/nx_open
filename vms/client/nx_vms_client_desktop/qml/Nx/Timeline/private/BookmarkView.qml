// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

import "."

import nx.vms.client.core

Item
{
    id: control

    property var bookmark

    signal tagClicked(string tag)

    implicitHeight: contentLayout.implicitHeight
    implicitWidth: contentLayout.implicitWidth

    ScrollView
    {
        id: scrollView
        anchors.fill: parent
        contentWidth: contentLayout.width

        ScrollBar.vertical: ScrollBar
        {
            id: verticalScrollBar
            parent: scrollView
            x: scrollView.width - width
            y: scrollView.topPadding
            height: scrollView.availableHeight
            active: scrollView.ScrollBar.horizontal.active

            contentItem: Rectangle
            {
                implicitWidth: 8
                color: ColorTheme.colors.timeline.bookmark.tooltip.scrollBar.indicator
            }

            background: Rectangle
            {
                color: ColorTheme.colors.timeline.bookmark.tooltip.scrollBar.background
            }
        }

        ColumnLayout
        {
            id: contentLayout
            width: scrollView.availableWidth
            spacing: 12

            RowLayout
            {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                Text
                {
                    Layout.fillWidth: true

                    text: control.bookmark.name
                    font.pixelSize: FontConfig.large.pixelSize
                    font.weight: FontConfig.large.weight
                    color: ColorTheme.colors.light4
                    wrapMode: Text.Wrap
                }

                ColoredImage
                {
                    visible: control.bookmark.shareable
                    sourcePath: "image://skin/20x20/Solid/shared.svg"
                    sourceSize: Qt.size(20, 20)
                    primaryColor: ColorTheme.colors.light4
                    secondaryColor: ColorTheme.colors.green

                    HoverHandler
                    {
                        id: hoverHandler
                    }

                    ToolTip.text: qsTr("Shared by link")
                    ToolTip.visible: hoverHandler.hovered
                    ToolTip.toolTip.popupType: Popup.Window
                }
            }

            Text
            {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: "%1 | %2"
                    .arg(NxGlobals.timeInShortFormat(control.bookmark.dateTime))
                    .arg(NxGlobals.dateInShortFormat(control.bookmark.dateTime))

                color: ColorTheme.colors.light4
                font.pixelSize: FontConfig.small.pixelSize
                font.weight: FontConfig.small.weight
            }

            LimitedFlow
            {
                id: tagFlow

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: control.bookmark.tags.length
                spacing: 4
                rowLimit: 1

                delegate: Tag
                {
                    text: model.tag
                    onClicked:
                    {
                        control.tagClicked(model.tag)
                    }
                }

                sourceModel: ListModel
                {
                    id: tagListModel
                }

                Tag
                {
                    text: "+" + tagFlow.remaining
                    onClicked:
                    {
                        tagFlow.rowLimit *= 3
                    }
                }

                Component.onCompleted:
                {
                    for (let tag of control.bookmark.tags)
                        tagListModel.append({"tag": tag})
                }
            }

            Text
            {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: control.bookmark.description
                color: ColorTheme.colors.light4
                wrapMode: Text.Wrap
                visible: text
            }
        }
    }
}
