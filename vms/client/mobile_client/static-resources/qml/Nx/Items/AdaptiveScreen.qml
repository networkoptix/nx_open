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

    default property alias contentData: container.data

    // Emitted whenever AdaptiveScreen closes a panel: the panel's close button was pressed, the
    // auto-close logic hid it because the content area would not fit, or `showPanel()` cross-closed
    // it to make room for the opposite panel. Consumers should release whatever state drives
    // `panel.item` (selection, persisted visibility, etc.) — otherwise re-selecting the same
    // entity will not retrigger `onItemChanged` and the panel will not reopen.
    signal panelClosed(Item panel)

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

    property Item leftPanelButtonContainer
    property Item rightPanelButtonContainer

    readonly property bool leftPanelButtonWanted: LayoutController.hasSidePanels
        && !LayoutController.fullscreen
        && root.hasLeftPanel
        && leftPanel.interactive
        && leftPanel.isHidden

    readonly property bool rightPanelButtonWanted: LayoutController.hasSidePanels
        && !LayoutController.fullscreen
        && root.hasRightPanel
        && rightPanel.interactive
        && rightPanel.isHidden

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

    // Opens `panel`, cross-closing the opposite one first if the layout cannot fit both
    // with the minimum content area. Use this from screens that drive `panel.visible` imperatively
    // (e.g. when the user selects an item and the matching panel should become visible).
    function showPanel(panel)
    {
        if (LayoutController.fullscreen)
            return

        d.lastOpenedPanel = panel

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
    }

    Item
    {
        id: defaultToolbarControlsContainer

        visible: false

        ToolBarButton
        {
            id: menuButton

            icon.source: "image://skin/24x24/Outline/more.svg?primary=%1"
                .arg(StyleHints.foregroundColorName)
            visible: false
        }

        ToolBarButton
        {
            // On the single pane layout toggles splash if has one, or pops the current item from
            // the stack if there is no splash and stack size > 1.
            id: defaultLeftControl

            visible: state !== ""
            anchors.centerIn: parent

            states:
            [
                State
                {
                    name: "openSplash"
                    when: root.hasSplash && !LayoutController.hasSidePanels

                    PropertyChanges
                    {
                        defaultLeftControl.icon.source: root.leftControlIconSource
                            || "image://skin/20x20/Solid/arrow_open.svg?primary=%1"
                                .arg(StyleHints.foregroundColorName)
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
        when: LayoutController.fullscreen
        restoreMode: Binding.RestoreBindingOrValue

        leftPanel.visible: false
        rightPanel.visible: false
    }

    ColumnLayout
    {
        id: singlePaneLayout

        anchors.fill: parent
        spacing: root.spacing
        visible: !LayoutController.hasSidePanels

        ProxyItem
        {
            Layout.fillWidth: true
            target: toolBar
            visible: !LayoutController.fullscreen
        }

        ProxyItem
        {
            objectName: "singlePaneContentProxyItem"

            Layout.fillWidth: true
            Layout.fillHeight: true

            target: root.contentItem
            background.color: ColorTheme.colors.dark4
        }
    }

    RowLayout
    {
        id: panelsLayout

        anchors.fill: parent
        spacing: root.spacing
        visible: LayoutController.hasSidePanels

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
                    visible: !LayoutController.fullscreen
                }

                ProxyItem
                {
                    objectName: "panelsContentProxyItem"

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

    NxControls.Button
    {
        id: leftPanelButton

        readonly property bool docked: !!root.leftPanelButtonContainer

        parent: root.leftPanelButtonContainer ?? root

        anchors.left: docked ? undefined : parent.left
        anchors.bottom: docked ? undefined : parent.bottom
        anchors.centerIn: docked ? parent : undefined
        anchors.leftMargin: 20
        anchors.bottomMargin: 12
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
        visible: root.leftPanelButtonWanted

        onClicked: root.showPanel(leftPanel)

        Indicator
        {
            id: leftPanelButtonIndicator

            anchors.topMargin: (parent.height - parent.icon.height) / 2 + 3
            anchors.rightMargin: (parent.width - parent.icon.width) / 2 + 2
            z: 1
            visible: false
        }
    }

    NxControls.Button
    {
        id: rightPanelButton

        readonly property bool docked: !!root.rightPanelButtonContainer

        parent: root.rightPanelButtonContainer ?? root

        anchors.right: docked ? undefined : parent.right
        anchors.bottom: docked ? undefined : parent.bottom
        anchors.centerIn: docked ? parent : undefined
        anchors.rightMargin: 20
        anchors.bottomMargin: 12
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
        visible: root.rightPanelButtonWanted

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

        function onHasSidePanelsChanged()
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

        // The panel `showPanel()` was called for most recently. When the layout stops fitting
        // both, this is the one that stays: the user has just asked for it. Tracked here rather
        // than via `visible`, which also changes when the whole panels layout is shown or hidden.
        property Item lastOpenedPanel: null

        // Layout width required to fit both side panels and the minimum content area.
        readonly property int requiredPanelsWidth: StyleHints.contentAreaMinimumWidth
            + leftPanel.implicitWidth
            + rightPanel.implicitWidth
            + 2 * root.spacing

        // True if the layout has enough room to host both side panels with at least
        // the minimum content area width visible between them. Independent of the panels
        // current visibility, so `showPanel()` can use it to decide whether opening one panel
        // must cross-close the opposite.
        readonly property bool fitsBothPanels: panelsLayout.width >= requiredPanelsWidth

        // Hides `panel` and notifies consumers via `panelClosed` so they can release whatever
        // state was driving it (selection, persisted visibility, etc.).
        function closePanel(panel)
        {
            if (lastOpenedPanel === panel)
                lastOpenedPanel = null

            panel.visible = false
            root.panelClosed(panel)
        }

        // Hides one of the panels when both are visible but the content area would not fit the
        // minimum width. Triggered imperatively from the Connections below rather than via a
        // reactive `onSomethingChanged` handler — modifying `visible` from a binding-change
        // handler causes QML to detect a binding loop (the handler writes a property that the
        // binding reads).
        function autoCloseIfNarrow()
        {
            if (!LayoutController.hasSidePanels)
                return

            if (!leftPanel.visible || !rightPanel.visible)
                return

            if (panelsLayout.width <= 0)
                return

            if (fitsBothPanels)
                return

            // Without a history to go by the right panel gives way, as it always did.
            closePanel(lastOpenedPanel === rightPanel ? leftPanel : rightPanel)
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
        target: panelsLayout
        function onWidthChanged() { d.autoCloseIfNarrow() }
    }
}
