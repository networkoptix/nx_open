// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Ui

import "private"

Drawer
{
    id: control

    property alias header: headerProxy.target
    property alias footer: footerProxy.target
    default property alias data: contentColumn.data

    property bool closeAutomatically: true
    property bool alwaysShowCloseButton: false
    readonly property alias availableContentHeight: flickable.height

    x: (parent.width - width) / 2
    edge: LayoutController.isPortrait ? Qt.BottomEdge : Qt.RightEdge
    width: LayoutController.isPortrait ? Math.min(parent.width, 640) : StyleHints.sheetWidth
    height:
    {
        if (!LayoutController.isPortrait)
            return parent.height

        const availableHeight = parent.height
            - parent.SafeArea.margins.top //< OS status bar.
            - parent.SafeArea.margins.bottom //< OS navigation bar.

        return Math.min(topPadding + bottomPadding + content.implicitHeight, availableHeight)
    }

    closePolicy: closeAutomatically
        ? Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        : Popup.NoAutoClose

    interactive: visible //< Swipe is available only for close.
    modal: true
    focus: true

    padding: 0
    topPadding: d.extraTopMargin
    bottomPadding: d.extraBottomMargin

    Overlay.modal: OverlayBackground
    {
        anchors.fill: parent
    }

    background: Item
    {
        id: backgroundItem

        readonly property int kCornerRadius: 10
        readonly property color kBackgroundColor: ColorTheme.colors.dark9

        Rectangle
        {
            anchors.top: parent.top
            width: parent.width
            height: 2 * radius
            radius: parent.kCornerRadius
            color: parent.kBackgroundColor
            visible: LayoutController.isPortrait
        }

        Rectangle
        {
            anchors.left: parent.left
            width: 2 * radius
            height: parent.height
            radius: parent.kCornerRadius
            color: parent.kBackgroundColor
            visible: !LayoutController.isPortrait
        }

        Rectangle
        {
            id: mainAreaBackground

            x: LayoutController.isPortrait ? 0 : parent.kCornerRadius
            y: LayoutController.isPortrait ? parent.kCornerRadius : 0
            width: parent.width - (LayoutController.isPortrait ? 0 : parent.kCornerRadius)
            height: parent.height - (LayoutController.isPortrait ? parent.kCornerRadius : 0)
            color: parent.kBackgroundColor
        }

        Rectangle
        {
            id: lineSeparator

            anchors.horizontalCenter: parent.horizontalCenter
            width: 42
            height: 3
            y: 8
            visible: LayoutController.isPortrait

            color: ColorTheme.colors.dark15
        }
    }

    contentItem: Item
    {
        id: content

        readonly property int kGradientSize: 80

        // Distance (in pixels of scrolling) over which the edge shadow fades in.
        readonly property int kShadowFadeZone: 24

        implicitHeight: flickable.anchors.topMargin
            + flickable.contentHeight
            + flickable.anchors.bottomMargin
        clip: true

        GradientShadow
        {
            id: topShadow

            width: parent.width
            height: content.kGradientSize + (header.visible ? headerProxy.y + headerProxy.height : 0)
            z: 5 //< Must be under header but over the flickable.

            distance: flickable.contentY
            fadeZone: content.kShadowFadeZone
            from: backgroundItem.kBackgroundColor
        }

        Item
        {
            id: header

            width: parent.width
            height: StyleHints.headerHeight
            visible: headerProxy.target
            z: 10

            RowLayout
            {
                anchors.fill: parent
                anchors.leftMargin: contentColumn.leftPadding
                anchors.rightMargin: contentColumn.rightPadding

                LayoutItemProxy
                {
                    id: headerProxy

                    Layout.fillWidth: true
                }

                LayoutItemProxy
                {
                    target: closeButton
                    visible: headerProxy.target
                        && (LayoutController.isTabletLayout || control.alwaysShowCloseButton)
                }
            }
        }

        Flickable
        {
            id: flickable

            anchors.fill: parent
            anchors.topMargin: header.visible ? header.height : 24
            anchors.bottomMargin: footer.visible ? footer.height : 20

            z: 1
            interactive: control.interactive && contentHeight > height
            contentHeight: contentColumn.height
            pressDelay: 50 //< Prefers scrolling over dragging the sheet.

            Item
            {
                id: scrollableContent

                height: contentColumn.height
                width: parent.width

                Column
                {
                    id: contentColumn

                    readonly property int leftPadding: 24
                    readonly property int rightPadding: 24

                    spacing: control.spacing
                    x: leftPadding
                    width: parent.width - leftPadding - rightPadding
                }
            }

            readonly property Item focusedItem:
            {
                let item = ApplicationWindow.activeFocusControl
                while (item && item.parent !== contentColumn)
                    item = item.parent

                return item
                    ? ApplicationWindow.activeFocusControl
                    : null
            }

            onFocusedItemChanged: ensureVisible(focusedItem)

            function ensureVisible(item)
            {
                // If for some reason user drags content or it jumps up or down (like with keyboard),
                // and we have this information with delay - we try to ensure that focused field is
                // visible after some reasonable delay.
                const ensureItemVisible =
                    () =>
                    {
                        if (!item || moving || dragging)
                            return

                        const ypos = item.mapToItem(contentItem, 0, 0).y
                        const ext = item.height + ypos
                        if (ypos < contentY
                            || ypos > contentY + height
                            || ext < contentY
                            || ext > contentY + height)
                        {
                            contentY = Math.max(0,
                                Math.min(ypos - height + item.height, contentHeight - height))
                        }
                    }

                const kReasonableDelayMs = 500
                CoreUtils.executeDelayed(ensureItemVisible, kReasonableDelayMs, flickable)
            }
        }

        Item
        {
            id: footer

            readonly property int kTopMargin: 24
            readonly property int kBottomMargin: 20

            width: parent.width
            height: kTopMargin + footerProxy.implicitHeight + kBottomMargin
            anchors.bottom: parent.bottom

            visible: footerProxy.target
            z: 10

            LayoutItemProxy
            {
                id: footerProxy

                x: contentColumn.leftPadding
                y: parent.kTopMargin
                width: parent.width - contentColumn.leftPadding - contentColumn.rightPadding
            }
        }

        GradientShadow
        {
            id: bottomShadow

            width: parent.width
            height: content.kGradientSize + (footer.visible ? footer.height - footer.kTopMargin : 0)
            anchors.bottom: parent.bottom
            z: 5 //< Must be under footer but over the flickable.

            rotation: 180
            distance: flickable.contentHeight - flickable.height - flickable.contentY
            fadeZone: content.kShadowFadeZone
            from: backgroundItem.kBackgroundColor
        }

        LayoutItemProxy
        {
            anchors.right: parent.right
            anchors.rightMargin: contentColumn.rightPadding
            y: flickable.y
            z: header.z + 1 //< Must be over header and flickable.

            target: closeButton
            visible: !headerProxy.target
                && (LayoutController.isTabletLayout || control.alwaysShowCloseButton)
        }

        IconButton
        {
            id: closeButton

            compact: true

            icon.source: "image://skin/24x24/Outline/close.svg?primary=light10"
            icon.width: 24
            icon.height: 24

            onClicked: control.close()
        }
    }

    onHeightChanged: flickable.ensureVisible(flickable.focusedItem)

    NxObject
    {
        id: d

        readonly property int extraTopMargin:
            (LayoutController.isPortrait ? 15 : 0) + control.SafeArea.margins.top

        readonly property int extraBottomMargin:
            d.keyboardHeight > 0 ? d.keyboardHeight : control.SafeArea.margins.bottom

        // Temporary solution. Will be fixed properly after UI refactor.
        readonly property real keyboardHeight: Qt.platform.os === "android"
            ? windowContext.ui.measurements.androidKeyboardHeight
            : (Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0)

    }
}
