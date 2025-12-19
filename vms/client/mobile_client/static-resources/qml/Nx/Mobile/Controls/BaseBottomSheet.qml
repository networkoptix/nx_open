// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

import "private"

Drawer
{
    id: control

    default property alias data: contentColumn.data

    property bool closeAutomatically: true

    property real extraBottomPadding: 20

    edge: Qt.BottomEdge

    width: parent.width
    height:
    {
        const maxHeight = Math.min(
            flickable.contentHeight + control.topPadding + control.bottomPadding,
            parent.height - windowContext.ui.measurements.deviceStatusBarHeight)
        return maxHeight
    }

    closePolicy: closeAutomatically
        ? Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        : Popup.NoAutoClose

    modal: true
    focus: true

    leftPadding: 0
    rightPadding: 0

    topPadding: 22 + 8
    bottomPadding:
    {
        const customPadding = d.keyboardHeight > 0
            ? 0
            : windowParams.bottomMargin
        return extraBottomPadding + customPadding + d.keyboardHeight
    }

    Overlay.modal: OverlayBackground { }

    background: Item
    {
        Rectangle
        {
            id: roundPartBackground

            anchors.top: parent.top
            width: parent.width
            height: 2 * radius
            radius: 10
            color: ColorTheme.colors.dark9
        }

        Rectangle
        {
            id: mainAreaBackground

            y: 10
            width: parent.width
            height: parent.height - y
            color: roundPartBackground.color
        }

        Rectangle
        {
            id: lineSeparator

            anchors.horizontalCenter: parent.horizontalCenter
            width: 42
            height: 3
            y: 8

            color: ColorTheme.colors.dark15
        }
    }

    contentItem: Flickable
    {
        id: flickable

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
