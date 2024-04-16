// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Controls
import Nx.Core

Item
{
    id: control

    property alias model: remoteAccessGrid.model
    
    Placeholder
    {
        id: placeholder

        visible: !remoteAccessGrid.visible
        imageSource: "image://skin/placeholders/services_placeholder.svg"
        text: qsTr("No services")
        additionalText: qsTr("Server is not configured for remote access feature")
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

                    Text
                    {
                        id: title
                        text: tile.name == "" ? qsTr("Unknown") : tile.name
                        color: ColorTheme.colors.light4
                        font.pixelSize: 24
                        font.weight: Font.Medium
                        font.capitalization: tile.name == "" ? Font.MixedCase : Font.AllUppercase
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
                        text: qsTr("Port")
                        font: grid.font
                        color: ColorTheme.colors.light16

                        Layout.preferredWidth: grid.firstColumnWidth
                        Layout.preferredHeight: grid.rowHeight
                    }

                    CopyableLabel
                    {
                        text: tile.port == "" ? "—" : tile.port
                        font: grid.font
                        color: ColorTheme.colors.light10
                        width: grid.secondColumnWidth

                        Layout.preferredHeight: grid.rowHeight
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
                        color: ColorTheme.colors.light10
                        width: grid.secondColumnWidth

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
                        color: ColorTheme.colors.light10
                        width: grid.secondColumnWidth
                        isPassword: true

                        Layout.preferredHeight: grid.rowHeight
                    }
                }
            }
        }
    }
}
