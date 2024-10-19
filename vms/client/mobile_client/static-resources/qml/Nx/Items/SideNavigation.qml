// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core
import Nx.Mobile
import Nx.Ui

import nx.vms.client.mobile

import "private/SideNavigation"

Drawer
{
    id: sideNavigation

    readonly property real offset:
    {
        switch (Screen.orientation)
        {
            case Qt.LandscapeOrientation:
            case Qt.InvertedLandscapeOrientation:
                return mainWindow.leftPadding
            default:
                return 0
        }
    }

    position: 0
    interactive: stackView.depth === 1
    width: offset + Math.min(
        ApplicationWindow.window.width - 56,
        ApplicationWindow.window.height - 56,
        56 * 6)
    height: ApplicationWindow.window.height - y

    Overlay.modal: Rectangle
    {
        color: ColorTheme.colors.backgroundDimColor
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.colors.dark8
        clip: true

        Item
        {
            x: sideNavigation.offset
            width: parent.width - x
            height: parent.height

            ListView
            {
                id: layoutsList

                anchors.fill: parent
                anchors.bottomMargin: bottomContent.height
                bottomMargin: 8
                flickableDirection: Flickable.VerticalFlick
                clip: true

                header: Column
                {
                    width: layoutsList.width

                    CloudPanel { }

                    SystemInformationPanel
                    {
                        visible: sessionManager.hasActiveSession
                    }
                }

                model: sessionManager.hasActiveSession ? layoutsModel : undefined

                delegate: LayoutItem
                {
                    text: resourceName
                    resourceId: uuid
                    shared: shared
                    active: uiController.layoutId === resourceId
                    type: itemType
                    count: itemsCount
                    onClicked:
                    {
                        Workflow.openResourcesScreen()
                        uiController.layoutId = resourceId
                    }
                }

                section.property: "section"
                section.labelPositioning: ViewSection.InlineLabels
                section.delegate: Item
                {
                    width: layoutsList.width
                    height: 32

                    Rectangle
                    {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        height: 1
                        color: ColorTheme.colors.dark10
                    }
                }
            }

            QnLayoutsModel { id: layoutsModel }

            OfflineDummy
            {
                id: disconnectedDummy

                anchors.fill: layoutsList
                anchors.margins: 16

                visible: !sessionManager.hasSession
            }

            Column
            {
                id: bottomContent

                width: parent.width
                bottomPadding: mainWindow.bottomPadding + 8
                anchors.bottom: parent.bottom

                BottomSeparator
                {
                    visible: disconnectButton.visible
                }

                SideNavigationButton
                {
                    icon: lp("/images/plus.png")
                    text: qsTr("New connection")
                    visible: disconnectedDummy.visible
                    onClicked:
                    {
                        bottomContent.enabled = false
                        sideNavigation.close()
                        Workflow.openNewSessionScreen()
                    }
                }

                SideNavigationButton
                {
                    id: disconnectButton

                    icon: lp("/images/disconnect.png")
                    text: qsTr("Disconnect from Server")
                    visible: !disconnectedDummy.visible
                    onClicked:
                    {
                        bottomContent.enabled = false
                        sessionManager.stopSession()
                    }
                }

                SideNavigationButton
                {
                    icon: lp("/images/settings.png")
                    text: qsTr("Settings")
                    onClicked:
                    {
                        bottomContent.enabled = false
                        sideNavigation.close()
                        Workflow.openSettingsScreen()
                    }
                }

                VersionLabel {}
            }
        }
    }

    onOpenedChanged:
    {
        if (opened)
        {
            bottomContent.enabled = true
            forceActiveFocus()
        }
        else
        {
            Workflow.focusCurrentScreen()
        }
    }

    Keys.onPressed:
    {
        if (CoreUtils.keyIsBack(event.key))
        {
            close()
            event.accepted = true
        }
    }
}
