// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window
import QtQuick.Controls

import Nx.Core
import Nx.Controls

import nx.vms.client.core

import "private"

TextInput
{
    id: control

    property bool showError: false

    property bool enableCustomHandles: Qt.platform.os == "android"

    property color inactiveColor: showError ? ColorTheme.colors.red_d1 : ColorTheme.colors.dark10
    property color activeColor: showError ? ColorTheme.colors.red_core : ColorTheme.colors.brand_core
    property color placeholderColor: ColorTheme.colors.dark10
    property color cursorColor: activeColor
    property bool selectionAllowed: true
    scrollByMouse: CoreUtils.isMobile()
    selectByMouse: !CoreUtils.isMobile()
    color: showError ? ColorTheme.colors.red_core : ColorTheme.windowText

    leftPadding: 8
    rightPadding: 8
    height: 48
    verticalAlignment: TextInput.AlignVCenter

    font.pixelSize: 16
    opacity: enabled ? 1.0 : 0.3

    property real backgroundRightOffset: 0

    background: Item
    {
        Rectangle
        {
            width: parent.width
            anchors
            {
                bottom: parent ? parent.bottom : undefined
                bottomMargin: 4
                left: parent ? parent.left : undefined
                leftMargin: 4
                right: parent ? parent.right : undefined
                rightMargin: 4 - control.backgroundRightOffset
            }
            height: 1
            color: control.activeFocus ? activeColor : inactiveColor
        }

        Rectangle
        {
            anchors
            {
                left: parent ? parent.left : undefined
                leftMargin: 4
                bottom: parent ? parent.bottom : undefined
                bottomMargin: 4
            }
            border.width: 0
            height: 6
            width: 1
            color: control.activeFocus ? activeColor : inactiveColor
        }

        Rectangle
        {
            anchors
            {
                right: parent ? parent.right : undefined
                rightMargin: 4 - control.backgroundRightOffset
                bottom: parent ? parent.bottom : undefined
                bottomMargin: 4
            }
            border.width: 0
            height: 6
            width: 1
            color: control.activeFocus ? activeColor : inactiveColor
        }
    }

    cursorDelegate: Rectangle
    {
        id: cursor
        width: 1
        color: cursorColor

        Timer
        {
            interval: 500
            running: control.cursorVisible && !cursorHandle.pressed
            repeat: true
            onTriggered: cursor.visible = !cursor.visible
            onRunningChanged:
            {
                cursor.visible = running || cursorHandle.pressed || contextMenu.visible
            }
        }
    }

    Text
    {
        id: placeholder

        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)

        text: control.placeholderText
        font: control.font
        color: control.placeholderColor
        horizontalAlignment: control.horizontalAlignment
        verticalAlignment: control.verticalAlignment
        visible: control.displayText === ""
        elide: Text.ElideRight
    }

    QtObject
    {
        id: d

        property bool selectionHandlesVisible: false
        property bool needRestoreContextMenu: false

        function showCursorHandle()
        {
            if (!enableCustomHandles)
                return

            cursorHandle.opacity = 0.0
            cursorHandle.visible = true
            cursorHandle.opacity = 1.0
        }

        function hideCursorHandle()
        {
            if (!enableCustomHandles)
                return

            cursorHandle.opacity = 0.0
            cursorHandle.visible = false
        }

        function updateCursorHandle()
        {
            if (!activeFocus && !contextMenu.activeFocus)
                hideCursorHandle()
        }

        function updateSelectionHandles()
        {
            if (!enableCustomHandles)
                return

            if (cursorHandle.pressed)
                return

            if (control.selectionStart == control.selectionEnd)
            {
                selectionHandlesVisible = false
            }
            else
            {
                hideCursorHandle()
                selectionHandlesVisible = true

                if (contextMenu.visible)
                    needRestoreContextMenu = true
                contextMenu.close()
            }
        }

        onSelectionHandlesVisibleChanged:
        {
            if (selectionHandlesVisible)
            {
                selectionStartHandle.opacity = 0.0
                selectionEndHandle.opacity = 0.0
                selectionStartHandle.visible = true
                selectionEndHandle.visible = true
                selectionEndHandle.opacity = 1.0
                selectionStartHandle.opacity = 1.0
            }
            else
            {
                selectionStartHandle.opacity = 0.0
                selectionEndHandle.opacity = 0.0
                selectionStartHandle.visible = false
                selectionEndHandle.visible = false
            }
        }

        function ensureAnchorVisible(anchor)
        {
            ensureVisible(control[anchor])
        }

        function selectWordAndOpenContextMenu(event, autoSelect)
        {
            if (!enableCustomHandles)
                return

            selectAndOpenMenuTimer.x = event.x
            selectAndOpenMenuTimer.y = event.y
            selectAndOpenMenuTimer.autoSelect = autoSelect
            selectAndOpenMenuTimer.restart()
        }

        function openContextMenu()
        {
            persistentSelection = true
            needRestoreContextMenu = false
            contextMenu.open()
            contextMenu.adjustPosition()
        }
    }

    onClicked:
    {
        if (!selectAndOpenMenuTimer.running && enableCustomHandles)
            openMenuTimer.restart()
        if (displayText && selectionStart == selectionEnd)
            d.showCursorHandle()
    }

    onPressAndHold: d.selectWordAndOpenContextMenu(event, true)
    onDoubleClicked: d.selectWordAndOpenContextMenu(event, true)

    onActiveFocusChanged: d.updateCursorHandle()

    onTextChanged: d.hideCursorHandle()

    // TODO: #dklychkov Finish implementation
    onSelectionStartChanged:
    {
        if (selectionStart == selectionEnd - 1 && !selectionEndHandle.insideInputBounds)
            d.ensureAnchorVisible("selectionEnd")
        else
            d.ensureAnchorVisible("selectionStart")
        d.updateSelectionHandles()
    }
    onSelectionEndChanged:
    {
        if (selectionEnd == selectionStart + 1 && !selectionStartHandle.insideInputBounds)
            d.ensureAnchorVisible("selectionStart")
        else
            d.ensureAnchorVisible("selectionEnd")
        d.updateSelectionHandles()
    }

    onCursorPositionChanged:
    {
        contextMenu.close()
    }

    Timer
    {
        id: openMenuTimer

        property int lastCursorPosition: -1
        property bool allowMenu: true

        interval: 250
        onTriggered:
        {
            if (selectAndOpenMenuTimer.running)
                return

            if (allowMenu && lastCursorPosition == cursorPosition)
            {
                allowMenu = !allowMenu
                d.openContextMenu()
                return
            }

            allowMenu = true
            lastCursorPosition = cursorPosition
        }
    }

    Timer
    {
        id: selectAndOpenMenuTimer

        property real x
        property real y
        property bool autoSelect

        interval: 150

        onTriggered:
        {
            openMenuTimer.stop()
            openMenuTimer.allowMenu = false

            var pos = positionAt(x, y)
            if (selectionStart < selectionEnd
                && (pos < selectionStart || pos > selectionEnd))
            {
                cursorPosition = pos
            }
            else if (autoSelect)
            {
                cursorPosition = pos
                selectWord()
            }

            d.openContextMenu()
        }
    }

    ScenePositionListener
    {
        id: positionListener
        item: control
    }

    InputHandle
    {
        id: cursorHandle
        anchor: "cursorPosition"
        input: control
        autoHide: !contextMenu.visible
        x: positionListener.scenePos.x + localX
        y: positionListener.scenePos.y + localY
        parent: control
    }

    InputHandle
    {
        id: selectionStartHandle
        anchor: "selectionStart"
        input: control
        x: positionListener.scenePos.x + localX
        y: positionListener.scenePos.y + localY
        parent: control

        onPressedChanged:
        {
            if (!pressed)
                d.openContextMenu()
        }
    }

    InputHandle
    {
        id: selectionEndHandle
        anchor: "selectionEnd"
        input: control
        x: positionListener.scenePos.x + localX
        y: positionListener.scenePos.y + localY
        parent: control

        onPressedChanged:
        {
            if (!pressed)
                d.openContextMenu()
        }
    }

    Menu
    {
        id: contextMenu

        modal: false
        focus: false

        MenuItem
        {
            text: qsTr("Cut")
            enabled: control.selectedText.length > 0
            onTriggered: control.cut()
        }

        MenuItem
        {
            text: qsTr("Copy")
            enabled: control.selectedText.length > 0
            onTriggered: control.copy()
        }

        MenuItem
        {
            text: qsTr("Paste")
            enabled: control.canPaste
            onTriggered: control.paste()
        }

        MenuItem
        {
            text: qsTr("Select All")
            enabled: control.text &&
                (control.selectionStart > 0 || control.selectionEnd < control.text.length)
            onTriggered: control.selectAll()
        }

        onActiveFocusChanged: d.updateCursorHandle()


        function adjustPosition()
        {
            x = leftPadding + positionToRectangle(
                selectionStart == selectionEnd
                    ? cursorPosition : selectionStart).x
            var yOffset = cursorRectangle.height + (enableCustomHandles ? 28 : 4)
            y = cursorRectangle.y + yOffset

            var window = control.ApplicationWindow.window
            if (!window)
                return

            var rect = mapToItem(window.contentItem, x, y, implicitWidth, implicitHeight)

            var dy = 0.0
            if (rect.bottom > window.height)
            {
                var topSpace = rect.top - yOffset - 4.0
                if (topSpace >= rect.height)
                    dy = -yOffset - rect.height - 4.0
                else
                    dy = window.height - rect.bottom - 8.0
            }
            if (rect.top + dy < 0)
                dy = -rect.top
            y += dy

            var dx = 0.0
            if (rect.right > window.width)
                dx = window.width - rect.right - 8.0
            if (rect.left + dx < 0)
                dx = -rect.left
            x += dx
        }

        onImplicitWidthChanged: adjustPosition()
        onImplicitHeightChanged: adjustPosition()
        onClosed: control.persistentSelection = false
    }

    Component.onCompleted:
    {
        if (Qt.platform.os == "android")
            passwordCharacter = "\u2022"
    }
}
