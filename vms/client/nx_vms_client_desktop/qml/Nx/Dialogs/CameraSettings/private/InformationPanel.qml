// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Controls 1.0

Control
{
    id: informationPanel

    property var engineInfo

    property bool checkable: true
    property bool checked: false
    property bool refreshing: false
    property bool refreshable: checked || !checkable

    property alias streamSelectorVisible: streamSelection.visible
    property alias streamSelectorEnabled: streamSelection.enabled
    property alias currentStreamIndex: streamComboBox.currentIndex

    signal enableSwitchClicked()
    signal refreshButtonClicked()

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

            RowLayout
            {
                id: caption

                width: parent.width
                height: 20
                spacing: 8

                SwitchIcon
                {
                    id: enableSwitch

                    visible: informationPanel.checkable
                    hovered: mouseArea.containsMouse && !mouseArea.containsPress
                    checkState: informationPanel.checked ? Qt.Checked : Qt.Unchecked
                    Layout.alignment: Qt.AlignBaseline

                    MouseArea
                    {
                        id: mouseArea

                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        hoverEnabled: true

                        onClicked:
                            informationPanel.enableSwitchClicked()
                    }
                }

                Text
                {
                    id: name

                    text: engineInfo ? engineInfo.name : ""
                    color: ColorTheme.text
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignBaseline
                    Layout.fillWidth: true
                }

                TextButton
                {
                    id: refreshButton

                    text: qsTr("Refresh")
                    icon.source: "qrc:///skin/text_buttons/refresh.png"
                    visible: informationPanel.refreshable && !informationPanel.refreshing

                    onClicked:
                        informationPanel.refreshButtonClicked()
                }

                RowLayout
                {
                    id: refreshingIndicator

                    spacing: 2
                    visible: informationPanel.refreshable && informationPanel.refreshing

                    AnimatedImage
                    {
                        source: "qrc:///skin/legacy/loading.gif"
                    }

                    Text
                    {
                        text: qsTr("Refreshing...")
                        font: refreshButton.font
                        color: refreshButton.color
                    }
                }
            }

            Text
            {
                id: description

                text: engineInfo ? engineInfo.description : ""
                color: ColorTheme.windowText
                width: parent.width
                visible: !!text
                wrapMode: Text.Wrap
            }
        }

        Grid
        {
            id: informationTable

            columns: 2
            columnSpacing: 8
            rowSpacing: 8
            flow: GridLayout.LeftToRight
            Layout.fillWidth: true

            Text
            {
                text: qsTr("Version")
                color: ColorTheme.windowText
                visible: version.visible
            }

            Text
            {
                id: version
                text: engineInfo ? engineInfo.version : ""
                color: ColorTheme.light
                visible: !!text
            }

            Text
            {
                text: qsTr("Vendor")
                color: ColorTheme.windowText
                visible: vendor.visible
            }

            Text
            {
                id: vendor
                text: engineInfo ? engineInfo.vendor : ""
                color: ColorTheme.light
                visible: !!text
            }
        }

        Column
        {
            id: streamSelection

            spacing: 16
            Layout.fillWidth: true

            Rectangle
            {
                id: separator
                width: parent.width
                height: 1
                color: ColorTheme.colors.dark13
            }

            Row
            {
                width: parent.width
                spacing: 8

                ContextHintLabel
                {
                    id: streamLabel

                    text: qsTr("Camera stream")
                    horizontalAlignment: Text.AlignRight
                    contextHintText: qsTr("Select video stream from the camera for analysis")
                    color: ColorTheme.windowText
                    width: informationPanel.width * 0.3 - informationPanel.leftPadding
                    y: streamComboBox.baselineOffset - baselineOffset
                }

                ComboBox
                {
                    id: streamComboBox

                    x: streamLabel.width
                    width: parent.width - x
                    model: ["Primary", "Secondary"]
                }
            }
        }
    }
}
