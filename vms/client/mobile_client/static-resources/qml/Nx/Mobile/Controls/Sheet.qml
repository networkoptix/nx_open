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
    readonly property int headerHeight: 56

    parent: Overlay.overlay

    width: parent.width
    height: parent.height

    modal: true

    background: Rectangle
    {
        color: ColorTheme.colors.dark7
    }

    focus: true
    topPadding: windowParams.topMargin
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 16 + windowParams.bottomMargin

    contentItem: Item
    {
        Item
        {
            id: header

            x: windowParams.leftMargin
            width: parent.width - windowParams.leftMargin - windowParams.rightMargin
            height: headerHeight

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

        Item
        {
            y: header.height + 12

            width: parent.width
            height: parent.height - y
            clip: true

            Column
            {
                id: contentColumn

                readonly property int leftPadding: 16 + windowParams.leftMargin
                readonly property int rightPadding: 16 + windowParams.rightMargin

                spacing: control.spacing
                x: leftPadding
                width: parent.width - leftPadding - rightPadding
            }
        }
    }
}
