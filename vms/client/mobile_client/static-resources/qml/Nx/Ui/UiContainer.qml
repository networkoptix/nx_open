// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls

import Nx.Controls
import Nx.Core
import Nx.Items
import Nx.Mobile
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

    ColumnLayout
    {
        id: bottomBarLayout

        anchors.fill: parent
        spacing: 1
        visible: !LayoutController.hasSidePanels

        ProxyItem
        {
            id: contentProxyItem

            Layout.fillWidth: true
            Layout.fillHeight: true

            target: stackView
        }
        ProxyItem
        {
            id: bottomNavigationBarProxy

            Layout.fillWidth: true

            target: screenNavigationBar

            visible: !LayoutController.fullscreen
                && !stackView.longContent
                && windowContext.sessionManager.hasActiveSession
                && [Controller.ResourcesScreen,
                    Controller.EventSearchScreen,
                    Controller.FeedScreen,
                    Controller.MenuScreen].includes(windowContext.deprecatedUiController.currentScreen)
        }
    }

    RowLayout
    {
        id: railLayout

        anchors.fill: parent
        spacing: 1
        visible: LayoutController.hasSidePanels
        clip: true

        ProxyItem
        {
            id: railNavigationBarProxy

            Layout.fillHeight: true
            target: screenNavigationBar

            visible: !LayoutController.fullscreen
                && windowContext.sessionManager.hasActiveSession
                && [Controller.ResourcesScreen,
                    Controller.EventSearchScreen,
                    Controller.FeedScreen,
                    Controller.SettingsScreen].includes(windowContext.deprecatedUiController.currentScreen)
        }

        ProxyItem
        {
            id: railContentProxy

            Layout.fillWidth: true
            Layout.fillHeight: true
            target: stackView
        }
    }

    // A popup rather than an item: the screens' own popups, sheets and the window banner live in
    // the window overlay above the content, and the notice must cover them as well.
    Controls.Popup
    {
        parent: Controls.Overlay.overlay
        width: parent?.width ?? 0
        height: parent?.height ?? 0
        z: 2 //< Should be above all the other popups and the window banner.

        modal: true
        dim: false
        focus: true
        closePolicy: Controls.Popup.NoAutoClose
        visible: LayoutController.isUnsupported

        // The popup's own SafeArea is not recalculated on device rotation while it is open, the
        // overlay's one is.
        topPadding: parent?.SafeArea.margins.top ?? 0
        leftPadding: parent?.SafeArea.margins.left ?? 0
        rightPadding: parent?.SafeArea.margins.right ?? 0
        bottomPadding: parent?.SafeArea.margins.bottom ?? 0
        background: Rectangle { color: ColorTheme.colors.dark4 }

        // Scrolls when the window is too short even for the notice itself.
        contentItem: Flickable
        {
            id: noticeFlickable

            contentWidth: width
            contentHeight: notice.height
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Placeholder
            {
                id: notice

                width: noticeFlickable.width
                height: Math.max(implicitHeight, noticeFlickable.height)

                imageSource: "image://skin/64x64/Outline/expand.svg?primary=light16"
                text: qsTr("Screen size is too small")
                description: qsTr(
                    "There isn't enough screen space. Resize the window or use a larger screen.")
                textColor: ColorTheme.colors.light12
                descriptionColor: ColorTheme.colors.light16
            }
        }
    }

    StackView
    {
        id: stackView

        objectName: "mainStackView"

        readonly property bool longContent: currentItem?.longContent || false

        function restoreActiveFocus()
        {
            if (activeFocusItem === mainWindow.contentItem)
                Workflow.focusCurrentScreen()
        }

        focus: true
        onCurrentItemChanged:
        {
            mainWindow.color = currentItem.hasOwnProperty("backgroundColor")
                ? currentItem.backgroundColor
                : ColorTheme.colors.dark4

            // Defer focus on currentItem to prevent a closing Menu from restoring focus to the
            // previous screen.
            Qt.callLater(() => currentItem.forceActiveFocus())
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
