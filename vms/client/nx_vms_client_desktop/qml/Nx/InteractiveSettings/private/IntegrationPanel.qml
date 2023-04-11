// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

PanelBase
{
    id: panel

    required property var engineInfo

    property bool checkable: true
    property bool checked: false
    property bool refreshing: false
    property bool refreshable: false

    property alias streamSelectorVisible: panel.contentVisible
    property alias streamSelectorEnabled: streamSelection.enabled
    property alias currentStreamIndex: streamComboBox.currentIndex

    signal enableSwitchClicked()
    signal refreshButtonClicked()

    enableButtonVisible: false
    removeButtonVisible: false

    headerItem: IntegrationHeader
    {
        name: engineInfo ? engineInfo.name : ""
        checkable: panel.checkable
        checked: panel.checked
        refreshing: panel.refreshing
        refreshable: panel.refreshable
        onEnableSwitchClicked: panel.enableSwitchClicked()
        onRefreshButtonClicked: panel.refreshButtonClicked()
    }

    description: engineInfo ? engineInfo.description : ""

    sourceItem: Grid
    {
        columns: 2
        columnSpacing: 8
        rowSpacing: 8
        flow: GridLayout.LeftToRight

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

    content: Row
    {
        id: streamSelection

        spacing: 8

        ContextHintLabel
        {
            id: streamLabel

            text: qsTr("Camera stream")
            horizontalAlignment: Text.AlignRight
            contextHintText: qsTr("Select video stream from the camera for analysis")
            color: ColorTheme.windowText
            width: panel.width * 0.3 - panel.leftPadding
            y: streamComboBox.baselineOffset - baselineOffset
        }

        ComboBox
        {
            id: streamComboBox

            width: parent.width - x
            model: ["Primary", "Secondary"]
        }
    }
}
