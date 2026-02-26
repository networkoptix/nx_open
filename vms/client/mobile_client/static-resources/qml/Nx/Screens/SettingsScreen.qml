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

    property string initialPage

    function ensureContentNotEmpty()
    {
        if (LayoutController.isTabletLayout)
        {
            if (!contentItem || contentItem === settingsNavigation)
                contentItem = interfaceSettingsPage
        }

        if (!contentItem)
            contentItem = settingsNavigation
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
                name: "returnToPreviousScreen"
                when: stackView.depth > 1
                    && (settingsScreen.initialPage
                        || LayoutController.isTabletLayout
                        || settingsScreen.contentItem === settingsNavigation)

                PropertyChanges
                {
                    backButton.onClicked: Workflow.popCurrentScreen()
                }
            },
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

            component SettingsNavigationItem: Button
            {
                property Item page

                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.preferredHeight: LayoutController.isTabletLayout ? 40 : 56

                leftPadding: LayoutController.isTabletLayout ? 8 : 16
                spacing: LayoutController.isTabletLayout ? 4 : 8
                textHorizontalAlignment: Text.AlignLeft

                text: page?.title ?? ""
                font.weight: Font.Normal
                font.pixelSize: LayoutController.isTabletLayout ? 14 : 18

                checked: settingsScreen.contentItem === page

                backgroundColor: LayoutController.isTabletLayout
                    ? (checked ? ColorTheme.colors.dark8 : "transparent")
                    : ColorTheme.colors.dark6

                foregroundColor: LayoutController.isTabletLayout
                    ? (checked ? ColorTheme.colors.light4 : ColorTheme.colors.light10)
                    : ColorTheme.colors.light1

                borderColor: backgroundColor

                radius: 8

                icon.width: 24
                icon.height: 24

                onClicked: settingsScreen.setContentItem(page)

                ColoredImage
                {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter

                    visible: !LayoutController.isTabletLayout
                    sourcePath: "image://skin/20x20/Outline/arrow_right.svg"
                    sourceSize: Qt.size(24, 24)
                    primaryColor: ColorTheme.colors.light16
                }
            }

            SettingsNavigationItem
            {
                Layout.topMargin: 20

                page: interfaceSettingsPage
                icon.source: "image://skin/24x24/Solid/interface.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                page: securitySettingsPage
                icon.source: "image://skin/24x24/Solid/security.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                page: performanceSettingsPage
                icon.source: "image://skin/24x24/Solid/speed.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                page: betaFeaturesPage
                icon.source: "image://skin/24x24/Solid/beta_features.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                icon.source: "image://skin/24x24/Solid/notifications.svg?primary=light1"
                text: qsTr("Notifications")
                page: pushExpertModePage
            }

            SettingsNavigationItem
            {
                page: appInfoPage
                icon.source: "image://skin/24x24/Solid/info.svg?primary=light1"
            }

            SettingsNavigationItem
            {
                page: developerSettingsPage
                icon.source: "image://skin/24x24/Solid/developer_settings.svg?primary=light1"
                visible: settingsScreen.contentItem === page
            }
        }
    }

    InterfaceSettingsPage
    {
        id: interfaceSettingsPage
        objectName: "interfaceSettingsPage"
    }

    SecuritySettingsPage
    {
        id: securitySettingsPage
        objectName: "securitySettingsPage"
    }

    PerformanceSettingsPage
    {
        id: performanceSettingsPage
        objectName: "performanceSettingsPage"
    }

    BetaFeaturesPage
    {
        id: betaFeaturesPage
        objectName: "betaFeaturesPage"
    }

    PushExpertModePage
    {
        id: pushExpertModePage
        objectName: "pushExpertModePage"
    }

    AppInfoPage
    {
        id: appInfoPage

        objectName: "appInfoPage"

        onDeveloperSettingsRequested: settingsScreen.contentItem = developerSettingsPage
    }

    DeveloperSettingsPage
    {
        id: developerSettingsPage
        objectName: "developerSettingsPage"
    }

    Connections
    {
        target: LayoutController

        function onIsTabletLayoutChanged()
        {
            settingsScreen.ensureContentNotEmpty()
        }
    }

    Component.onCompleted:
    {
        contentItem = data.find((i) => i.objectName && i.objectName === initialPage) ?? null

        ensureContentNotEmpty()
    }
}
