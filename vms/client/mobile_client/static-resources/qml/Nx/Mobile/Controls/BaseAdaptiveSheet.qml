// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Ui

import "private"

Drawer
{
    id: control

    property alias footer: footerProxy.target
    default property alias data: contentColumn.data

    property bool closeAutomatically: true

    property real extraBottomPadding: 20

    x: (parent.width - width) / 2
    edge: LayoutController.isTabletLayout ? Qt.RightEdge : Qt.BottomEdge
    width: LayoutController.isTabletLayout ? 400 : Math.min(parent.width, 640)
    height:
    {
        const availableHeight = parent.height - windowContext.ui.measurements.deviceStatusBarHeight

        if (LayoutController.isTabletLayout)
            return availableHeight


        return Math.min(
            flickable.contentHeight + (content.footer ? content.footer.height + spacing : 0) + control.topPadding + control.bottomPadding,
            availableHeight)
    }

    closePolicy: closeAutomatically
        ? Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        : Popup.NoAutoClose

    modal: true
    focus: true

    leftPadding: 0
    rightPadding: 0

    topPadding: 22 + (LayoutController.isTabletLayout ? 0 : 8) //< TODO: Sync the given padding with the figma.
    bottomPadding:
    {
        const customPadding = d.keyboardHeight > 0
            ? 0
            : windowParams.bottomMargin
        return extraBottomPadding + customPadding + d.keyboardHeight
    }

    Overlay.modal: OverlayBackground
    {
        anchors.fill: parent
    }

    background: Item
    {
        readonly property int kCornerRadius: 10
        readonly property color kBgColor: ColorTheme.colors.dark9

        Rectangle
        {
            anchors.top: parent.top
            width: parent.width
            height: 2 * radius
            radius: parent.kCornerRadius
            color: parent.kBgColor
            visible: !LayoutController.isTabletLayout
        }

        Rectangle
        {
            anchors.left: parent.left
            width: 2 * radius
            height: parent.height
            radius: parent.kCornerRadius
            color: parent.kBgColor
            visible: LayoutController.isTabletLayout
        }

        Rectangle
        {
            id: mainAreaBackground

            x: LayoutController.isTabletLayout ? parent.kCornerRadius : 0
            y: LayoutController.isTabletLayout ? 0 : parent.kCornerRadius
            width: parent.width - (LayoutController.isTabletLayout ? parent.kCornerRadius : 0)
            height: parent.height - (LayoutController.isTabletLayout ? 0 : parent.kCornerRadius)
            color: parent.kBgColor
        }

        Rectangle
        {
            id: lineSeparator

            anchors.horizontalCenter: parent.horizontalCenter
            width: 42
            height: 3
            y: 8
            visible: !LayoutController.isTabletLayout

            color: ColorTheme.colors.dark15
        }
    }

    contentItem: Page
    {
        id: content

        padding: 0
        background: Item{}

        footer: Item
        {
            implicitHeight: footerProxy.height + control.spacing
            visible: footerProxy.target

            LayoutItemProxy
            {
                id: footerProxy
                y: control.spacing
                x: contentColumn.leftPadding
                width: parent.width - contentColumn.leftPadding - contentColumn.rightPadding
            }
        }

        Flickable
        {
            id: flickable

            anchors.fill: parent
            anchors.bottomMargin: content.footer ? control.spacing : 0
            clip: true
            contentHeight: contentColumn.height

            Item
            {
                id: scrollableContent

                height: contentColumn.height
                width: parent.width

                Column
                {
                    id: contentColumn

                    readonly property int leftPadding: 20 + windowParams.leftMargin
                    readonly property int rightPadding: 20 + windowParams.rightMargin

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
    }

    onHeightChanged: flickable.ensureVisible(flickable.focusedItem)

    NxObject
    {
        id: d

        // Temporary solution. Will be fixed properly after UI refactor.
        readonly property real keyboardHeight: Qt.platform.os === "android"
            ? windowContext.ui.measurements.androidKeyboardHeight
            : (Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0)

    }
}
