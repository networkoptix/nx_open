// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls

import Nx.Controls
import Nx.Core
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Mobile.Ui
import Nx.Ui

// The given component is intended for positioning items depending on the application's layout.
// On the mobile layout, the screen is divided into three parts. There is a `TopBar` at the top,
// a `Content` in the center, and a `NavigationBar` at the bottom. Also, there is an optional
// `Splash`, which is called by the left button in the `TopBar`.
// On the tablet mode, the layout is divided as follows: `NavigationBar` is on the left, `TopBar`
// is on the top (but between the left and right panels), and there are three sections
// in the center: left panel (optional), content and right panel (optional).
// To fill these sections, `AdaptiveScreenItem` should be used; any other component will take up
// the entire screen.
Item
{
    id: uiContainer

    property alias stackView: stackView
    property alias navigationBar: screenNavigationBar

    WarningPanel
    {
        id: topLevelWarning

        anchors.left: parent.left
        anchors.right: parent.right
        text: d.warningText
        opened: text.length
    }

    ColumnLayout
    {
        id: mobileLayout

        anchors.topMargin: topLevelWarning.height
        anchors.fill: parent
        spacing: 1
        visible: !LayoutController.isTabletLayout

        ProxyItem
        {
            id: mobileContentProxyItem

            Layout.fillWidth: true
            Layout.fillHeight: true

            target: stackView
        }
        ProxyItem
        {
            id: mobileNavigationBarProxy

            Layout.fillWidth: true

            target: screenNavigationBar

            visible: !stackView.fullscreen
                && !stackView.longContent
                && windowContext.sessionManager.hasSession
                && [Controller.ResourcesScreen,
                    Controller.EventSearchScreen,
                    Controller.FeedScreen,
                    Controller.MenuScreen].includes(windowContext.deprecatedUiController.currentScreen)
        }
    }

    RowLayout
    {
        id: tabletLayout

        anchors.topMargin: topLevelWarning.height
        anchors.fill: parent
        spacing: 1
        visible: LayoutController.isTabletLayout
        clip: true

        ProxyItem
        {
            id: tabletNavigationBarProxy

            Layout.fillHeight: true
            target: screenNavigationBar

            visible: !stackView.fullscreen
                && windowContext.sessionManager.hasSession
                && [Controller.ResourcesScreen,
                    Controller.EventSearchScreen,
                    Controller.FeedScreen,
                    Controller.SettingsScreen].includes(windowContext.deprecatedUiController.currentScreen)
        }

        ProxyItem
        {
            id: tabletContentProxy

            Layout.fillWidth: true
            Layout.fillHeight: true
            target: stackView
        }
    }

    StackView
    {
        id: stackView

        objectName: "mainStackView"

        readonly property bool fullscreen: currentItem?.fullscreen || false
        readonly property bool longContent: currentItem?.longContent || false

        function restoreActiveFocus()
        {
            if (activeFocusItem == Window.contentItem)
                Workflow.focusCurrentScreen()
        }

        focus: true
        onCurrentItemChanged:
        {
            mainWindow.color = currentItem.hasOwnProperty("backgroundColor")
                ? currentItem.backgroundColor
                : ColorTheme.colors.dark4
        }
        onWidthChanged: autoScrollDelayTimer.restart()
        onHeightChanged: autoScrollDelayTimer.restart()
    }

    UiController
    {
        stackView: stackView
    }

    ScreenNavigationBar
    {
        id: screenNavigationBar
    }
}
