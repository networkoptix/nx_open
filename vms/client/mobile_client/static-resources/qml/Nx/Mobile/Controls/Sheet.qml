// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Controls
import Nx.Mobile.Ui

Popup
{
    id: control

    default property alias data: contentColumn.data
    property alias title: sheetTitleText.text
    property alias contentSpacing: contentColumn.spacing

    implicitWidth: mainWindow.width
    implicitHeight: mainWindow.height

    background: Rectangle
    {
        color: ColorTheme.colors.dark7
    }

    leftPadding: 16
    rightPadding: 16
    bottomPadding: 16

    contentItem: Item
    {
        Item
        {
            id: header

            x: -control.leftPadding
            width: parent.width + control.leftPadding + control.rightPadding
            height: 56

            TitleLabel
            {
                id: sheetTitleText

                x: closeButton.width + 8
                width: parent.width - x * 2
                anchors.verticalCenter: parent.verticalCenter
            }

            IconButton
            {
                id: closeButton

                x: parent.width - width
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://skin/20x20/Outline/close_medium.svg?primary=light10"
                icon.width: 20
                icon.height: 20

                onClicked: control.close()
            }
        }

        Flickable
        {
            y: header.height + 12

            width: parent.width
            height: parent.height - y - control.bottomPadding
            clip: true

            contentHeight: contentColumn.height

            Column
            {
                id: contentColumn

                spacing: 4
                width: parent.width
            }
        }
    }
}
