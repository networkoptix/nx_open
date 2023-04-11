// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

Control
{
    id: informationPanel

    property alias headerItem: headerControl.contentItem
    property alias description: description.text
    property alias sourceItem: sourceControl.contentItem
    property alias content: content.contentItem

    property bool enableButtonVisible: false
    property bool removeButtonVisible: false
    property bool contentVisible: true

    signal enableClicked()
    signal removeClicked()

    padding: 12

    background: Rectangle { color: ColorTheme.colors.dark8 }

    contentItem: ColumnLayout
    {
        id: column
        spacing: 16

        clip: true

        Column
        {
            spacing: 8
            Layout.fillWidth: true

            Control
            {
                id: headerControl

                width: parent.width
            }

            Text
            {
                id: description

                font.pixelSize: 14
                color: ColorTheme.windowText
                width: parent.width
                visible: !!text
                wrapMode: Text.Wrap
            }
        }

        Control
        {
            id: sourceControl

            Layout.fillWidth: true
            visible: !!contentItem
        }

        Row
        {
            spacing: 8
            visible: enableButtonVisible || removeButtonVisible

            Button
            {
                id: enableButton

                visible: enableButtonVisible
                text: qsTr("Approve")
                onClicked: informationPanel.enableClicked()
            }

            Button
            {
                id: removeButton

                visible: removeButtonVisible
                text: qsTr("Reject")
                onClicked: informationPanel.removeClicked()
            }
        }

        Rectangle
        {
            id: separator

            Layout.fillWidth: true
            height: 1
            color: ColorTheme.colors.dark13
            visible: content.contentItem && content.contentItem.visible
        }

        Control
        {
            id: content

            Layout.fillWidth: true
            width: parent.width
            visible: contentItem && contentVisible
        }
    }
}
