// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

import nx.vms.client.desktop

Item
{
    id: control

    property alias model: remoteAccessGrid.model
    
    Placeholder
    {
        id: placeholder

        visible: !remoteAccessGrid.visible || !control.model.enabledForCurrentUser
        imageSource: control.model.enabledForCurrentUser
            ? "image://skin/64x64/Outline/noservices.svg"
            : "image://skin/64x64/Outline/no_access.svg"
        text: control.model.enabledForCurrentUser ? qsTr("No services") : qsTr("Disabled")
        additionalText: control.model.enabledForCurrentUser
            ? qsTr("Server is not configured for remote access feature")
            : qsTr("Remote access tool is currently disabled. Please enable it or contact your "
                + "administrator to turn it on.")
        maxWidth: 250

        readonly property string url: "" //TODO: #esobolev add link (see VMS-51876)

        anchors.verticalCenterOffset: 0

        Component
        {
            id: helpComponent

            Window
            {
                width: 1024
                height: 750
                modality: Qt.ApplicationModal

                WebEngineView
                {
                    url: placeholder.url
                    anchors.fill: parent
                }
            }
        }
        
        Text
        {
            textFormat: Text.RichText
            text: "<a href=\"%1\">%2</a>".arg(placeholder.url).arg(qsTr("Learn more"))

            Layout.alignment: Qt.AlignHCenter

            onLinkActivated:
            {
                const helpTools = helpComponent.createObject()

                helpTools.show()
                helpTools.raise()
            }
        }
    }

    GridView
    {
        id: remoteAccessGrid

        property real spacing: 12
        cellHeight: 144 + spacing
        cellWidth: 309 + spacing
        visible: count > 0
        anchors.fill: parent

        delegate: Rectangle
        {
            id: tile

            required property string name
            required property string port
            required property string login
            required property string password

            height: 144
            width: 309
            color: ColorTheme.colors.dark8
            radius: 4

            border.color: ColorTheme.colors.dark10
            border.width: 1

            ColumnLayout
            {
                spacing: 12
                anchors.fill: parent
                anchors.margins: 16

                ColumnLayout
                {
                    height: 36
                    spacing: 7

                    Item
                    {
                        id: tileHeader

                        Layout.preferredHeight: title.height
                        Layout.fillWidth: true

                        Text
                        {
                            id: title
                            text: tile.name ?? qsTr("Unknown")
                            color: ColorTheme.colors.light4
                            anchors.left: tileHeader.left
                            font.pixelSize: 24
                            font.weight: Font.Medium
                            font.capitalization: tile.name ? Font.AllUppercase : Font.MixedCase
                        }

                        ContextHintButton
                        {
                            anchors.verticalCenter: title.verticalCenter
                            anchors.right: tileHeader.right
                            toolTipText: qsTr("Connect %1 client application to localhost:%2")
                                .arg(title.text).arg(portLabel.text)

                            helpTopic: HelpTopic.Empty //TODO: #esobolev add link (see VMS-51876)
                        }
                    }

                    Rectangle
                    {
                        height: 1
                        color: ColorTheme.colors.dark11
                        
                        Layout.fillWidth: true
                    }
                }

                GridLayout
                {
                    id: grid
                    columns: 2
                    rowSpacing: 4

                    readonly property int firstColumnWidth: 88
                    readonly property int secondColumnWidth: 190
                    readonly property int rowHeight: 20
                    readonly property var font: FontConfig.normal

                    Text
                    {
                        text: qsTr("Local port")
                        font: grid.font
                        color: ColorTheme.colors.light16

                        Layout.preferredWidth: grid.firstColumnWidth
                        Layout.preferredHeight: grid.rowHeight
                    }

                    Item
                    {
                        Layout.preferredHeight: grid.rowHeight

                        CopyableLabel
                        {
                            id: portLabel
                            text: tile.port
                            font: grid.font
                            color: hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light10
                            width: grid.secondColumnWidth
                            visible: !!tile.port
                            copiedTooltipLifetimeMs: 2500
                        }

                        Item
                        {
                            visible: !portLabel.visible

                            ColoredImage
                            {
                                id: errorImage
                                sourcePath: "image://skin/16x16/Solid/error.svg"
                                primaryColor: ColorTheme.colors.red_l
                            }

                            Text
                            {
                                text: qsTr("An error occurred")
                                color: ColorTheme.colors.red_l
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                anchors.left: errorImage.right
                                anchors.leftMargin: 4
                            }
                        }
                    }

                    Text
                    {
                        text: qsTr("Username")
                        font: grid.font
                        color: ColorTheme.colors.light16

                        Layout.preferredWidth: grid.firstColumnWidth
                        Layout.preferredHeight: grid.rowHeight
                    }

                    CopyableLabel
                    {
                        text: tile.login == "" ? "—" : tile.login
                        font: grid.font
                        color: hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light10
                        width: grid.secondColumnWidth
                        copiedTooltipLifetimeMs: 2500

                        Layout.preferredHeight: grid.rowHeight
                    }

                    Text
                    {
                        text: qsTr("Password")
                        font: grid.font
                        color: ColorTheme.colors.light16

                        Layout.preferredWidth: grid.firstColumnWidth
                        Layout.preferredHeight: grid.rowHeight
                    }

                    CopyableLabel
                    {
                        text: tile.password == "" ? "—" : tile.password
                        font: grid.font
                        color: hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light10
                        width: grid.secondColumnWidth
                        isPassword: true
                        copiedTooltipLifetimeMs: 2500

                        Layout.preferredHeight: grid.rowHeight
                    }
                }
            }
        }
    }
}
