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

FocusScope
{
    id: root

    default property alias data: container.data

    // Emitted whenever AdaptiveScreen closes a panel: the panel's close button was pressed, the
    // auto-close logic hid it because the content area would not fit, or `showPanel()` cross-closed
    // it to make room for the opposite panel. Consumers should release whatever state drives
    // `panel.item` (selection, persisted visibility, etc.) — otherwise re-selecting the same
    // entity will not retrigger `onItemChanged` and the panel will not reopen.
    signal panelClosed(Item panel)

    // Whether only the contentItem is visible.
    property bool fullscreen: false

    // Whether tool bar visible in the fullscreen mode.
    property bool fullscreenToolBar: false

    // Whether the screen content require a lot of space. The given property is a hint for
    // the parent container to provide the maximum amount of available space.
    property bool longContent: false

    property alias toolBar: toolBar
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

    property var customBackHandler

    property bool isActive: StackView.status === StackView.Active
    readonly property bool hasSplash: splashItem
    readonly property bool hasLeftPanel: leftPanel.item
    readonly property bool hasRightPanel: rightPanel.item

    readonly property int spacing: 1

    // Opens `panel`, cross-closing the opposite one first if the landscape layout cannot fit both
    // with the minimum content area. Use this from screens that drive `panel.visible` imperatively
    // (e.g. when the user selects an item and the matching panel should become visible).
    function showPanel(panel)
    {
        const opposite = panel === leftPanel ? rightPanel : leftPanel
        if (opposite.visible && !d.fitsBothPanels)
            d.closePanel(opposite)

        panel.visible = true
    }

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

        implicitHeight: StyleHints.headerHeight
        titleUnderlineVisible: false
        leftControl: root.customLeftControl ?? defaultLeftControl
        rightControl: root.customRightControl
            ? [root.customRightControl, menuButton]
            : [menuButton]
        useGradientBackground: root.fullscreen
    }

    Item
    {
        id: defaultToolbarControlsContainer

        visible: false

        ToolBarButton
        {
            id: menuButton

            icon.source: "image://skin/24x24/Outline/more.svg?primary=light4"
            visible: false
        }

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
        spacing: root.spacing
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
        spacing: root.spacing
        visible: LayoutController.isTabletLayout

        Panel
        {
            id: leftPanel

            readonly property bool isHidden: !visible

            Layout.fillHeight: true
            visible: false

            onCloseButtonClicked: d.closePanel(leftPanel)

            Binding
            {
                when: !leftPanel.item
                restoreMode: Binding.RestoreBindingOrValue

                leftPanel.visible: false
            }
        }

        Item
        {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout
            {
                anchors.fill: parent
                spacing: root.spacing

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
            visible: false

            onCloseButtonClicked: d.closePanel(rightPanel)

            Binding
            {
                when: !rightPanel.item
                restoreMode: Binding.RestoreBindingOrValue

                rightPanel.visible: false
            }
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

        onClicked: root.showPanel(leftPanel)

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

        onClicked: root.showPanel(rightPanel)

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
        visible: false
        topPadding: SafeArea.margins.top
        leftPadding: SafeArea.margins.left
        rightPadding: SafeArea.margins.right
        bottomPadding: SafeArea.margins.bottom
        background: Rectangle { color: splashPanel.color }

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

        function onIsTabletLayoutChanged()
        {
            splash.close()
        }
    }

    Keys.onPressed: (event) =>
    {
        if (!CoreUtils.keyIsBack(event.key))
            return

        event.accepted = true

        if (root.customBackHandler)
            root.customBackHandler(event.key === Qt.Key_Escape)
        else if (stackView.depth > 1)
            Workflow.popCurrentScreen()
    }

    QtObject
    {
        id: d

        // Landscape layout width required to fit both side panels and the minimum content area.
        readonly property int requiredLandscapeWidth: StyleHints.contentAreaMinimumWidth
            + leftPanel.implicitWidth
            + rightPanel.implicitWidth
            + 2 * root.spacing

        // True if the landscape layout has enough room to host both side panels with at least
        // the minimum content area width visible between them. Independent of the panels
        // current visibility, so `showPanel()` can use it to decide whether opening one panel
        // must cross-close the opposite.
        readonly property bool fitsBothPanels: landscapeLayout.width >= requiredLandscapeWidth

        // Hides `panel` and notifies consumers via `panelClosed` so they can release whatever
        // state was driving it (selection, persisted visibility, etc.).
        function closePanel(panel)
        {
            panel.visible = false
            root.panelClosed(panel)
        }

        // Hides the right panel when both panels are visible but the content area would not fit
        // the minimum width. Triggered imperatively from the Connections below rather than via a
        // reactive `onSomethingChanged` handler — modifying `rightPanel.visible` from a binding-
        // change handler causes QML to detect a binding loop (the handler writes a property that
        // the binding reads).
        function autoCloseIfNarrow()
        {
            if (!LayoutController.isTabletLayout)
                return

            if (!leftPanel.visible || !rightPanel.visible)
                return

            if (landscapeLayout.width <= 0)
                return

            if (fitsBothPanels)
                return

            closePanel(rightPanel)
        }
    }

    Connections
    {
        target: leftPanel
        function onVisibleChanged() { d.autoCloseIfNarrow() }
    }

    Connections
    {
        target: rightPanel
        function onVisibleChanged() { d.autoCloseIfNarrow() }
    }

    Connections
    {
        target: landscapeLayout
        function onWidthChanged() { d.autoCloseIfNarrow() }
    }
}
