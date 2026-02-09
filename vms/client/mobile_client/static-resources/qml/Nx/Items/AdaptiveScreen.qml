// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls as NxControls
import Nx.Items
import Nx.Mobile.Controls
import Nx.Mobile.Ui
import Nx.Ui

Item
{
    id: root

    default property alias data: container.data

    // Whether only the contentItem is visible.
    property bool fullscreen
    // Whether tool bar visible in the fullscreen mode.
    property bool fullscreenToolBar

    property alias toolBar: toolBar //< TODO: the given alias must be removed.
    property alias title: toolBar.title
    property alias titleUnderlineVisible: toolBar.titleUnderlineVisible

    // The given item is a content item of the screen, resides it the center.
    required property Item contentItem

    // The given item is a container for items resides over all the adaptive screen.
    property alias overlayItem: overlay.target

    property alias splash: splash
    // Optional item, which will be placed on the popup on the mobile layout.
    property alias splashItem: splashPanel.item
    property string splashTitle

    property alias leftPanel: leftPanel
    property alias leftPanelButtonIndicator: leftPanelButtonIndicator

    property alias rightPanel: rightPanel
    property alias rightPanelButtonIndicator: rightPanelButtonIndicator

    property string leftControlIconSource
    property alias leftControlEnabled: defaultLeftControl.enabled

    // If default left and right button controls does not fit, the given properties allows override
    // controls used by default.
    property Item customLeftControl
    property Item customRightControl

    property alias menuButton: menuButton

    property var customToolBarClickHandler
    property var customBackHandler

    property bool isActive: StackView.status === StackView.Active
    readonly property bool hasSplash: splashItem
    readonly property bool hasLeftPanel: leftPanel.item
    readonly property bool hasRightPanel: rightPanel.item

    Item
    {
        // All the children mush be hidden by default to prevent occasion placing on the background.
        // Visibility must be controlled by the proxy items only.
        id: container

        objectName: "adaptiveScreenItemsContainer"

        visible: false
    }

    NxControls.ToolBar
    {
        id: toolBar

        titleUnderlineVisible: false
        leftControl: root.customLeftControl ?? defaultLeftControl
        rightControl: root.customRightControl
        useGradientBackground: root.fullscreen

        centerControl: ToolBarButton
        {
            id: menuButton

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            icon.source: "image://skin/24x24/Outline/more.svg?primary=light4"
            visible: false
        }

        onToolBarClicked:
        {
            if (root.customToolBarClickHandler)
                root.customToolBarClickHandler()
        }
    }

    Item
    {
        id: defaultToolbarControlsContainer

        visible: false

        ToolBarButton
        {
            // On the portrait layout toggles splash if has one or pop current item from the stack
            // if there is no splash and stack size > 1.
            id: defaultLeftControl

            visible: state !== ""

            states:
            [
                State
                {
                    name: "openSplash"
                    when: root.hasSplash && !LayoutController.isTabletLayout

                    PropertyChanges
                    {
                        defaultLeftControl.icon.source: root.leftControlIconSource
                            || "image://skin/20x20/Solid/arrow_open.svg?primary=light10"
                        defaultLeftControl.onClicked: splash.open()
                    }
                },
                State
                {
                    name: "returnToPreviousScreen"
                    when: stackView.depth > 1

                    PropertyChanges
                    {
                        defaultLeftControl.icon.source: "qrc:////images/arrow_back.png"
                        defaultLeftControl.onClicked: Workflow.popCurrentScreen()
                    }
                }
            ]
        }
    }

    Binding
    {
        when: root.fullscreen
        restoreMode: Binding.RestoreBindingOrValue

        leftPanel.visible: false
        rightPanel.visible: false
    }

    ColumnLayout
    {
        id: portraitLayout

        anchors.fill: parent
        spacing: 0
        visible: !LayoutController.isTabletLayout

        ProxyItem
        {
            Layout.fillWidth: true
            target: toolBar
            visible: !root.fullscreen
        }

        ProxyItem
        {
            objectName: "portraitContentProxyItem"

            Layout.fillWidth: true
            Layout.fillHeight: true

            target: root.contentItem
            background.color: ColorTheme.colors.dark4
        }
    }

    RowLayout
    {
        id: landscapeLayout

        anchors.fill: parent
        spacing: 2
        visible: LayoutController.isTabletLayout

        Panel
        {
            id: leftPanel

            readonly property bool isHidden: !visible

            Layout.fillHeight: true
            visible: !!item
            onCloseButtonClicked: visible = false
        }

        Item
        {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout
            {
                anchors.fill: parent
                spacing: landscapeLayout.spacing

                ProxyItem
                {
                    Layout.fillWidth: true
                    target: toolBar
                    visible: !root.fullscreen
                }

                ProxyItem
                {
                    objectName: "landscapeContentProxyItem"

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    target: root.contentItem
                    background.color: ColorTheme.colors.dark4
                }
            }
        }

        Panel
        {
            id: rightPanel

            readonly property bool isHidden: !visible

            Layout.fillHeight: true
            visible: !!item
            onCloseButtonClicked: visible = false
        }
    }

    ProxyItem
    {
        id: fullscreenToolbarProxy

        width: parent.width
        target: toolBar
        background.color: "transparent"
        visible: root.fullscreen && root.fullscreenToolBar
    }

    NxControls.Button
    {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
        leftPadding: 0
        rightPadding: 0
        padding: 0
        width: 56
        height: 56
        color: ColorTheme.colors.light10
        radius: 16
        display: AbstractButton.IconOnly
        icon.source: leftPanel.iconSource
            || "image://skin/24x24/Solid/left_panel_open.svg?primary=dark1"
        icon.width: 24
        icon.height: 24
        visible: LayoutController.isTabletLayout
            && !root.fullscreen
            && root.hasLeftPanel
            && root.leftPanel.interactive
            && leftPanel.isHidden
        onClicked: leftPanel.visible = true

        Indicator
        {
            id: leftPanelButtonIndicator

            anchors.topMargin: (parent.height - parent.icon.height) / 2
            anchors.rightMargin: (parent.width - parent.icon.width) / 2
            z: 1
            visible: false
        }
    }

    NxControls.Button
    {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        leftPadding: 0
        rightPadding: 0
        padding: 0
        width: 56
        height: 56
        color: ColorTheme.colors.light10
        radius: 16
        display: AbstractButton.IconOnly
        icon.source: rightPanel.iconSource
            || "image://skin/24x24/Solid/right_panel_open.svg?primary=dark1"
        icon.width: 24
        icon.height: 24
        visible: LayoutController.isTabletLayout
            && !root.fullscreen
            && root.hasRightPanel
            && root.rightPanel.interactive
            && rightPanel.isHidden
        onClicked: rightPanel.visible = true

        Indicator
        {
            id: rightPanelButtonIndicator

            anchors.topMargin: (parent.height - parent.icon.height) / 2
            anchors.rightMargin: (parent.width - parent.icon.width) / 2
            z: 1
            visible: false
        }
    }

    Popup
    {
        id: splash

        parent: Overlay.overlay
        width: Overlay.overlay ? Overlay.overlay.width : 0
        height: Overlay.overlay ? Overlay.overlay.height : 0
        modal: true
        padding: 0
        visible: false

        contentItem: Panel
        {
            id: splashPanel

            color: ColorTheme.colors.dark4
            title: root.splashTitle
            onCloseButtonClicked: splash.close()
        }
    }

    ProxyItem
    {
        id: overlay

        anchors.fill: parent
        background.color: "transparent"
        visible: target
    }

    Connections
    {
        target: LayoutController

        function onIsPortraitChanged()
        {
            splash.close()
        }
    }

    Keys.onPressed: (event) =>
    {
        if (event.key === Qt.Key_F11)
        {
            root.fullscreen = !root.fullscreen
            event.accepted = true
            return
        }

        if (!CoreUtils.keyIsBack(event.key))
            return

        event.accepted = true

        if (root.customBackHandler)
            root.customBackHandler(event.key === Qt.Key_Escape)
        else if (stackView.depth > 1)
            Workflow.popCurrentScreen()
    }
}
