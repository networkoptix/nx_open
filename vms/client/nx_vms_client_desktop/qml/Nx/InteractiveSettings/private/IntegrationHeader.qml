// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

RowLayout
{
    id: header

    property alias name: name.text
    property bool checkable: false
    property bool checked: false
    property var offline: undefined
    property bool refreshing: false
    property bool refreshable: false

    signal enableSwitchClicked()
    signal refreshButtonClicked()

    implicitWidth: 100
    implicitHeight: 20
    spacing: 8

    SwitchIcon
    {
        id: enableSwitch

        visible: header.checkable
        hovered: mouseArea.containsMouse && !mouseArea.containsPress
        checkState: header.checked ? Qt.Checked : Qt.Unchecked
        Layout.alignment: Qt.AlignBaseline

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true

            onClicked:
                header.enableSwitchClicked()
        }
    }

    Text
    {
        id: name

        color: ColorTheme.text
        font.pixelSize: 16
        font.weight: Font.Medium
        elide: Text.ElideRight
        Layout.alignment: Qt.AlignBaseline
    }

    Text
    {
        id: status

        text: qsTr("OFFLINE")
        visible: header.offline === true

        color: ColorTheme.colors.red_core
        font.weight: Font.Medium
        font.pixelSize: 10
        elide: Text.ElideRight
        Layout.alignment: Qt.AlignBaseline
    }

    Item
    {
        Layout.fillWidth: true
    }

    TextButton
    {
        id: refreshButton

        text: qsTr("Refresh")
        icon.source: "image://svg/skin/text_buttons/reload_20.svg"
        visible: header.refreshable && !header.refreshing

        onClicked:
            header.refreshButtonClicked()
    }

    RowLayout
    {
        id: refreshingIndicator

        spacing: 2
        visible: header.refreshable && header.refreshing

        AnimatedImage
        {
            source: "qrc:///skin/legacy/loading.gif"
            Layout.alignment: Qt.AlignVCenter
        }

        Text
        {
            text: qsTr("Refreshing...")
            font: refreshButton.font
            color: refreshButton.color
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
