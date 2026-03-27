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
    property real extraBottomPadding: 0

    x: (parent.width - width) / 2
    edge: LayoutController.isPortrait ? Qt.BottomEdge : Qt.RightEdge
    width: LayoutController.isPortrait ? Math.min(parent.width, 640) : 400
    height:
    {
        if (!LayoutController.isPortrait)
            return parent.height

        const availableHeight = parent.height
            - parent.SafeArea.margins.top //< OS status bar.
            - parent.SafeArea.margins.bottom //< OS navigation bar.

        return Math.min(
            bottomPadding
                + content.anchors.topMargin
                + (control.header ? d.headerHeight : 0)
                + flickable.anchors.topMargin
                + flickable.contentHeight
                + flickable.anchors.bottomMargin
                + (control.footer ? control.footer.height : 0)
                + content.anchors.bottomMargin,
            availableHeight)
    }

    closePolicy: closeAutomatically
        ? Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        : Popup.NoAutoClose

    modal: true
    focus: true
    padding: 0
    topPadding: SafeArea.margins.top
    bottomPadding: d.keyboardHeight > 0 ? d.keyboardHeight : SafeArea.margins.bottom

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
            visible: LayoutController.isPortrait
        }

        Rectangle
        {
            anchors.left: parent.left
            width: 2 * radius
            height: parent.height
            radius: parent.kCornerRadius
            color: parent.kBgColor
            visible: !LayoutController.isPortrait
        }

        Rectangle
        {
            id: mainAreaBackground

            x: LayoutController.isPortrait ? 0 : parent.kCornerRadius
            y: LayoutController.isPortrait ? parent.kCornerRadius : 0
            width: parent.width - (LayoutController.isPortrait ? 0 : parent.kCornerRadius)
            height: parent.height - (LayoutController.isPortrait ? parent.kCornerRadius : 0)
            color: parent.kBgColor
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
        Page
        {
            id: content

            anchors.fill: parent
            anchors.topMargin: d.extraTopMargin
            anchors.bottomMargin: 24 + extraBottomPadding

            padding: 0
            background: Item {}

            header: Item
            {
                implicitHeight: d.headerHeight
                visible: headerProxy.target

                LayoutItemProxy
                {
                    id: headerProxy

                    x: contentColumn.leftPadding
                    y: 24
                    width: parent.width - contentColumn.leftPadding - contentColumn.rightPadding
                }
            }

            footer: Item
            {
                implicitHeight: footerProxy.height
                visible: footerProxy.target

                LayoutItemProxy
                {
                    id: footerProxy

                    x: contentColumn.leftPadding
                    width: parent.width - contentColumn.leftPadding - contentColumn.rightPadding
                }
            }

            Flickable
            {
                id: flickable

                property bool needScroll: contentHeight > height

                anchors.fill: parent
                anchors.topMargin: headerProxy.target ? control.spacing : 24
                anchors.bottomMargin: footerProxy.target ? control.spacing : 0

                interactive: control.interactive
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

            GradientShadow
            {
                id: topShadow

                x: flickable.x
                y: flickable.y
                width: flickable.width
                visible: flickable.needScroll && !flickable.atYBeginning
                from: ColorTheme.colors.dark9
            }

            GradientShadow
            {
                id: bottomShadow

                x: flickable.x
                y: flickable.height - height
                width: flickable.width
                rotation: 180
                visible: flickable.needScroll && !flickable.atYEnd
                from: ColorTheme.colors.dark9
            }
        }

        IconButton
        {
            id: closeButton

            // Align button with the sheet content or header by the icon boundaries.
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 8
            anchors.topMargin: control.header ? 12 + d.extraTopMargin : 8

            visible: LayoutController.isTabletLayout || control.alwaysShowCloseButton
            padding: 0

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

        readonly property int headerHeight: 64
        readonly property int extraTopMargin: LayoutController.isPortrait ? 15 : 0

        // Temporary solution. Will be fixed properly after UI refactor.
        readonly property real keyboardHeight: Qt.platform.os === "android"
            ? windowContext.ui.measurements.androidKeyboardHeight
            : (Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0)

    }
}
