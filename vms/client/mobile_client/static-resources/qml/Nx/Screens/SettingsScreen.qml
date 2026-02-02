// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Mobile.Controls
import Nx.Settings
import Nx.Ui

import nx.vms.client.core

import "private/SettingsScreen"

AdaptiveScreen
{
    id: settingsScreen

    objectName: "settingsScreen"

    // TODO: Add initialPage property.

    function checkTabletLayoutContentNotEmpty()
    {
        if (LayoutController.isTabletLayout)
        {
            if (!contentItem || contentItem === settingsNavigation)
                contentItem = interfaceSettingsPage
        }
    }

    function setContentItem(newItem)
    {
        if (contentItem === settingsNavigation)
        {
            contentItem = newItem
            return
        }

        saveSettingsHandler.target = contentItem
        saveSettingsHandler.queuedItem = newItem

        contentItem.saveSettings()
    }

    Connections
    {
        id: saveSettingsHandler

        property var queuedItem

        target: null

        function onSettingsSaved()
        {
            settingsScreen.contentItem = queuedItem
            settingsScreen.enabled = true
        }

        function onError()
        {
            settingsScreen.enabled = true
        }
    }

    customLeftControl: ToolBarButton
    {
        id: backButton

        anchors.centerIn: parent
        icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=light4"
        visible: state !== ""

        states:
        [
            State
            {
                name: "returnToSettingsNavigation"
                when: !LayoutController.isTabletLayout
                    && settingsScreen.contentItem !== settingsNavigation

                PropertyChanges
                {
                    backButton.onClicked: settingsScreen.setContentItem(settingsNavigation)
                }
            },
            State
            {
                name: "returnToPreviousScreen"
                when: stackView.depth > 1 &&
                    (LayoutController.isTablet
                        || (!LayoutController.isTabletLayout && settingsScreen.contentItem === settingsNavigation))

                PropertyChanges
                {
                    backButton.onClicked: Workflow.popCurrentScreen()
                }
            },
            State
            {
                name: "returnToMenuScreen"
                when: stackView.depth === 1 && !LayoutController.isTabletLayout

                PropertyChanges
                {
                    backButton.onClicked: Workflow.openMenuScreen()
                }
            }
        ]
    }

    customRightControl: contentItem?.rightControl ?? null

    title:
    {
        if (contentItem === settingsNavigation)
            return qsTr("Settings")

        return contentItem?.title ?? ""
    }

    contentItem: LayoutController.isTabletLayout ? null : settingsNavigation

    leftPanel
    {
        interactive: false
        title: qsTr("Settings")
        item: settingsNavigation
    }

    Flickable
    {
        id: settingsNavigation

        ColumnLayout
        {
            id: settingsNavigationContent

            width: settingsNavigation.width
            spacing: 4

            component SettingsNavigationItem: LabeledSwitch //< TODO: Use button component here.
            {
                property Item page
                property int margin: LayoutController.isTabletLayout ? 20 : 0

                horizontalPadding: LayoutController.isTabletLayout ? 8 : 16
                verticalPadding: LayoutController.isTabletLayout ? 8 : 12

                Layout.fillWidth: true
                Layout.leftMargin: margin
                Layout.rightMargin: margin

                text: page?.title ?? ""
                topPadding: verticalPadding
                bottomPadding: verticalPadding
                leftPadding: horizontalPadding
                rightPadding: horizontalPadding
                showIndicator: false
                backgroundColor: !LayoutController.isTabletLayout || settingsScreen.contentItem === page
                    ? ColorTheme.colors.dark6
                    : "transparent"
                backgroundRadius: settingsScreen.contentItem === page ? 4 : 0
                onClicked: settingsScreen.setContentItem(page)
                showCustomArea: !LayoutController.isTabletLayout
                customArea: ColoredImage
                {
                    anchors.verticalCenter: parent.verticalCenter
                    sourcePath: "image://skin/20x20/Outline/arrow_right.svg"
                    sourceSize: Qt.size(24, 24)
                    primaryColor: ColorTheme.colors.light16
                }
            }

            SettingsNavigationItem
            {
                id: interfaceSettingsItem

                Layout.topMargin: margin
                page: interfaceSettingsPage
                icon: "image://skin/24x24/Solid/interface.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                page: securitySettingsPage
                icon: "image://skin/24x24/Solid/security.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                page: performanceSettingsPage
                icon: "image://skin/24x24/Solid/speed.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                page: betaFeaturesPage
                icon: "image://skin/24x24/Solid/beta_features.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                id: notificationsSwitch

                icon: "image://skin/24x24/Solid/notifications.svg?primary=light1"
                text: qsTr("Notifications")
                page: pushExpertModePage
            }

            SettingsNavigationItem
            {
                id: appInfoOption

                Layout.bottomMargin: margin
                page: appInfoPage
                icon: "image://skin/24x24/Solid/info.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                id: developerSettings

                Layout.bottomMargin: margin
                page: developerSettingsPage
                icon: "image://skin/24x24/Solid/developer_settings.svg?primary=light1"
                visible: settingsScreen.contentItem === page
            }
        }
    }

    InterfaceSettingsPage
    {
        id: interfaceSettingsPage
    }

    SecuritySettingsPage
    {
        id: securitySettingsPage
    }

    PerformanceSettingsPage
    {
        id: performanceSettingsPage
    }

    BetaFeaturesPage
    {
        id: betaFeaturesPage
    }

    PushExpertModePage
    {
        id: pushExpertModePage
    }

    AppInfoPage
    {
        id: appInfoPage

        onDeveloperSettingsRequested: settingsScreen.contentItem = developerSettingsPage
    }

    DeveloperSettingsPage
    {
        id: developerSettingsPage
    }

    Connections
    {
        target: LayoutController

        function onIsTabletLayoutChanged()
        {
            settingsScreen.checkTabletLayoutContentNotEmpty()
        }
    }

    Component.onCompleted:
    {
        checkTabletLayoutContentNotEmpty()
    }
}
