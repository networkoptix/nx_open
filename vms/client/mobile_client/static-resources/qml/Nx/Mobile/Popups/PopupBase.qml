// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.15

import Nx.Controls 1.0
import Nx.Core 1.0

import "private"

Popup
{
    id: control

    readonly property int gap: 20
    property bool autoClose: false
    property alias icon: icon.source
    property alias title: title.text
    property var messages: []
    property alias buttonBoxButtons: buttonBox.data

    parent: mainWindow.contentItem
    anchors.centerIn: parent

    width: Math.min(328, parent ? parent.width - (gap * 2) : 0)
    height: Math.min(contentHeight, parent ? parent.height - (gap * 2) : 0)
    padding: 0

    closePolicy: autoClose
        ? Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside
        : Popup.NoAutoClose
    modal: true

    background: Rectangle
    {
        color: ColorTheme.colors.dark10
        radius: 6
    }

    contentHeight: (icon.visible ? icon.height + icon.Layout.topMargin : 0)
        + title.Layout.topMargin + title.height + title.Layout.bottomMargin
        + (separator.visible ? separator.height + separator.Layout.bottomMargin : 0)
        + popupTextColumn.height
        + contentLayout.anchors.bottomMargin
        + buttonBox.height + buttonBox.anchors.bottomMargin

    contentItem: Item
    {
        ColumnLayout
        {
            id: contentLayout
            width: parent.width
            anchors.top: parent.top
            anchors.bottom: buttonBox.top
            anchors.bottomMargin: 24
            spacing: 0

            Image
            {
                id: icon

                Layout.alignment: Qt.AlignCenter
                Layout.topMargin: 24

                visible: status !== Image.Null
                sourceSize.width: 48
                sourceSize.height: 48
            }

            Text
            {
                id: title

                Layout.fillWidth: true
                Layout.margins: 24
                Layout.bottomMargin: icon.visible ? 12 : 24
                Layout.rightMargin: icon.visible ? 24 : 24 + closeButton.width + 12

                text: control.title
                font.pixelSize: 20
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
                color: ColorTheme.colors.light4
                horizontalAlignment: icon.visible ? Text.AlignHCenter : Text.AlignLeft
            }

            PopupSeparator
            {
                id: separator

                Layout.fillWidth: true
                Layout.bottomMargin: 15

                visible: !icon.visible
            }

            ScrollView
            {
                id: scrollView

                property bool needScroll: contentHeight > height

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                ScrollBar.vertical: ScrollBar
                {
                    id: scrollBar
                    policy: ScrollBar.AlwaysOff
                }

                Column
                {
                    id: popupTextColumn
                    width: scrollView.width
                    spacing: 12

                    Repeater
                    {
                        model: control.messages
                        delegate: PopupText
                        {
                            text: modelData
                            width: parent.width
                            visible: text
                        }
                    }
                }
            }
        }

        GradientShadow
        {
            id: topShadow
            x: scrollView.x
            y: scrollView.y
            width: scrollView.width
            visible: scrollView.needScroll && scrollBar.position > 0
        }

        GradientShadow
        {
            id: bottomShadow
            x: scrollView.x
            y: contentLayout.height - height
            width: scrollView.width
            rotation: 180
            visible: scrollView.needScroll && scrollBar.size + scrollBar.position < 1
        }

        IconButton
        {
            id: closeButton
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 24

            icon.source: "image://skin/24x24/Outline/close.svg?primary=light10"

            width: 24
            height: 24
            icon.width: 24
            icon.height: 24

            onClicked: control.close()
        }

        Column
        {
            id: buttonBox
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 24
            spacing: 12
        }
    }
}
