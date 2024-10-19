// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Models
import Nx.Ui

import nx.vms.client.core

Page
{
    id: sessionsScreen
    objectName: "sessionsScreen"

    leftButtonIcon.source: lp("/images/menu.png")

    leftPadding: 12
    rightPadding: 12

    onLeftButtonClicked: sideNavigation.open()

    titleControls:
    [
        Button
        {
            text: qsTr("Log in to %1", "%1 is the short cloud name (like 'Cloud')").arg(applicationInfo.cloudName())
            textColor: ColorTheme.colors.dark16
            flat: true
            leftPadding: 0
            rightPadding: 0
            labelPadding: 8
            visible: cloudStatusWatcher.status == CloudStatusWatcher.LoggedOut
            onClicked: Workflow.openCloudLoginScreen()
        }
    ]

    MouseArea
    {
        id: searchCanceller

        z: 1
        anchors.fill: parent

        onPressed: (mouse) =>
        {
            mouse.accepted = false
            searchEdit.resetFocus()
        }
    }

    Rectangle
    {
        id: searchPanel

        color: sessionsScreen.backgroundColor

        readonly property bool searching:
            visible && (searchEdit.activeFocus || searchEdit.displayText.length)

        readonly property point searchPanelPosition:
        {
            if (searchPanel.searching)
                return Qt.point(0, 0)

            return sessionsList.mapToItem(
                searchPanel.parent,
                sessionsList.originX - sessionsList.contentX,
                sessionsList.originY - sessionsList.contentY)
        }

        x: searchPanelPosition.x
        y: searchPanelPosition.y
        z: 2

        height: searchEdit.y + searchEdit.height + 12
        width: sessionsList.width
        visible: sessionsList.model.acceptedRowCount > 8

        SearchEdit
        {
            id: searchEdit

            property Item placeholder: searchPanel.searching ? null : placeholderHolder
            property Item placeholderHolder: null

            y: 4
            height: 40
            width: parent.width
            onDisplayTextChanged:
            {
                sessionsList.model.filterWildcard = displayText
                if (!displayText.length)
                    sessionsList.positionViewAtBeginning()
            }
        }
    }

    GridView
    {
        id: sessionsList

        clip: true
        width: parent.width
        height: parent.height

        header: Item
        {
            width: sessionsList.width
            height: searchPanel.searching || searchPanel.visible ? searchPanel.height : 16
            Component.onCompleted: searchEdit.placeholderHolder = this
        }

        footer: Item
        {
            width: sessionsList.width
            height: 12
        }

        property real horizontalSpacing: 8
        property real verticalSpacing: cellsInRow > 1 ? 8 : 1
        readonly property real maxCellWidth: 488 + horizontalSpacing
        property int cellsInRow: Math.max(1, Math.ceil(width / maxCellWidth))

        cellWidth: (width - leftMargin - rightMargin) / cellsInRow
        cellHeight: 98 + verticalSpacing

        model: OrderedSystemsModel
        {
            id: systemsModel

            // TODO: #ynikitenkov refactor to use ConnectionValidator implementation instead.
            // minimalVersion: "3.0" //< Will be fixed soon.
            filterCaseSensitivity: Qt.CaseInsensitive
            filterRole: searchRoleId()
        }

        delegate: Item
        {
            width: sessionsList.cellWidth
            height: sessionsList.cellHeight

            SessionItem
            {
                readonly property bool firstRowItem: index < sessionsList.cellsInRow
                readonly property real verticalMargin: Math.floor(sessionsList.verticalSpacing / 2)
                readonly property real horizontalMargin: sessionsList.horizontalSpacing / 2

                anchors.fill: parent
                anchors.leftMargin: horizontalMargin
                anchors.rightMargin: horizontalMargin
                anchors.topMargin: firstRowItem ? 0 : verticalMargin
                anchors.bottomMargin: sessionsList.verticalSpacing - verticalMargin

                systemName: model.systemName
                systemId: model.systemId
                localId: model.localId
                cloudSystem: model.isCloudSystem
                factorySystem: model.isFactorySystem
                needDigestCloudPassword: !model.isOauthSupported && !hasDigestCloudPassword
                ownerDescription: cloudSystem ? model.ownerDescription : ""
                running: model.isOnline
                reachable: model.isReachable
                compatible: model.isCompatibleToMobileClient || model.isFactorySystem
                wrongCustomization: model.wrongCustomization ? model.wrongCustomization : false
                invalidVersion: model.wrongVersion ? model.wrongVersion.toString() : ""
            }
        }
        highlightMoveDuration: 0

        focus: false

        Keys.onPressed:
        {
            // TODO: #dklychkov Implement transparent navigation to the footer item.
            if (event.key == Qt.Key_C)
            {
                Workflow.openNewSessionScreen()
                event.accepted = true
            }
        }
    }

    footer: Rectangle
    {
        implicitHeight: customConnectionButton.implicitHeight + 16
        color: ColorTheme.colors.windowBackground

        Rectangle
        {
            width: parent.width
            height: 1
            anchors.top: parent ? parent.top : undefined
            color: ColorTheme.colors.dark4
        }

        Button
        {
            id: customConnectionButton

            text: dummyMessage.visible
                ? qsTr("Connect to Server...")
                : qsTr("Connect to Another Server...")

            anchors.centerIn: parent
            width: parent.width - 16

            onClicked: Workflow.openNewSessionScreen()
        }
    }

    DummyMessage
    {
        id: dummyMessage

        readonly property string kCheckNetworkMessage:
            qsTr("Check your network connection or press \"%1\" button to enter a known server address.",
             "%1 is a button name").arg(customConnectionButton.text)

        anchors.fill: parent
        anchors.bottomMargin: -8

        readonly property bool emptySearchMode:
            searchPanel.searching && sessionsList.model.sourceRowsCount > 0

        title: emptySearchMode ? qsTr("Nothing found") : qsTr("No Sites found")
        description: emptySearchMode ? "" : kCheckNetworkMessage

        visible: false

        Timer
        {
            interval: 1000
            running: true
            onTriggered:
            {
                dummyMessage.visible = Qt.binding(function() { return sessionsList.count == 0 })
            }
        }
    }

    function resetSearch()
    {
        searchEdit.clear()
        if (!sessionsList.count)
            return

        sessionsList.positionViewAtBeginning()
        sessionsList.contentY += searchPanel.visible ? searchEdit.height : 0
    }

    onVisibleChanged:
    {
        if (visible)
            resetSearch()
    }

    Component.onCompleted: resetSearch()
}
