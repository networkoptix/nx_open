// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Nx.Controls

import nx.vms.client.core

/**
 * Implements text selection behavior for TextEdit and TextInput controls.
 */
Item
{
    id: control

    required property var target
    property var selectText: (start, end) => {}
    property var selectWord: (position) => {}
    property bool menuEnabled: true

    readonly property bool draggingHandles: startHandle.dragging || endHandle.dragging

    component Handle: Image
    {
        id: handle

        property int textPosition: target.positionAt(point.x, point.y) //< Default binding.

        signal edited(int position)

        // Invisible active point of the handle.
        readonly property point point: Qt.point(x + pointOffset.x, y + pointOffset.y)
        readonly property point pointOffset: Qt.point(width / 2, -width / 2)
        readonly property rect textPositionRectangle: target.positionToRectangle(textPosition)
        readonly property bool dragging: mouseArea.drag.active

        readonly property bool isInDraggingArea:
            (() => control.contains(control.mapFromItem(target, point)))
                /* depends on */ (positionListener.scenePos)

        Binding on textPosition //< Overrides any external binding during dragging.
        {
            when: dragging
            value: control.target.positionAt(point.x, point.y)
        }

        onTextPositionChanged:
        {
            if (!dragging)
                snap()
        }

        visible: target.selectionStart !== target.selectionEnd
        opacity: dragging || isInDraggingArea ? 1.0 : 0.0

        source: lp("images/controls/cursor_handle.png")

        MouseArea
        {
            id: mouseArea
            anchors.fill: parent
            drag.target: handle
            drag.threshold: 0
            drag.smoothed: true
            drag.onActiveChanged:
            {
                if (!drag.active)
                    handle.snap()
            }
            onPositionChanged: (mouse) =>
            {
                if (drag.active)
                    handle.edited(textPosition)
            }
        }

        function snap()
        {
            const textPositionRectangle = target.positionToRectangle(textPosition)

            // Snap to the center of the text position rectangle.
            const snappedPoint = Qt.point(
                textPositionRectangle.x,
                textPositionRectangle.top + textPositionRectangle.height / 2)

            x = snappedPoint.x - pointOffset.x
            y = snappedPoint.y - pointOffset.y
        }

        Component.onCompleted: snap()
    }

    MouseArea
    {
        id: targetArea

        parent: target
        anchors.fill: parent

        onPressed: (event) => event.accepted = true
        onReleased: (event) => event.accepted = true

        onDoubleClicked: (event) => selectWord(event)
        onPressAndHold: (event) => selectWord(event)

        function selectWord(event)
        {
            if (!control.contains(mapToItem(control, event.x, event.y)))
                return

            if (!target.activeFocus)
                target.forceActiveFocus()

            control.selectWord(target.positionAt(event.x, event.y))
        }

        onClicked: (event) =>
        {
            if (!control.contains(mapToItem(control, event.x, event.y)))
                return

            if (!target.activeFocus)
                target.forceActiveFocus()

            // Opens the context menu when tapped on the same text position.
            const position = target.positionAt(event.x, event.y)
            if (target.cursorPosition === position)
                menu.open(position)
            else
                target.cursorPosition = position
        }
    }

    Handle
    {
        id: startHandle

        // Allows swapping handles during dragging.
        textPosition: !draggingHandles ? target.selectionStart : textPosition
        onEdited: updateTargetSelection()
        parent: targetArea
    }

    Handle
    {
        id: endHandle

        // Allows swapping handles during dragging.
        textPosition: !draggingHandles ? target.selectionEnd : textPosition
        onEdited: updateTargetSelection()
        parent: targetArea
    }

    function updateTargetSelection()
    {
        if (startHandle.textPosition !== endHandle.textPosition)
        {
            // Allows to swap during dragging the handles.
            selectText(
                Math.min(startHandle.textPosition, endHandle.textPosition),
                Math.max(startHandle.textPosition, endHandle.textPosition))
        }
    }

    // Helps update the visibility of the handles when the target is placed inside Flickable.
    ScenePositionListener
    {
        id: positionListener
        item: target
    }

    Menu
    {
        id: menu

        property int verticalOffset: 30

        opacity: !draggingHandles && menuEnabled ? 1.0 : 0.0
        popupType: Popup.Window
        modal: false
        focus: false

        function open(position)
        {
            x = position.x - width / 2
            y = position.y + verticalOffset
            showTimer.start()
        }

        function adjust()
        {
            const startRect = control.mapFromItem(target,
                target.positionToRectangle(target.selectionStart))

            const endRect = control.mapFromItem(target,
                target.positionToRectangle(target.selectionEnd))

            const multilineSelection = startRect.y !== endRect.y

            x = multilineSelection
                ? (control.width - menu.width) / 2
                : (endRect.x + startRect.x) / 2 - menu.width / 2

            y = Math.max(0, Math.min(endRect.bottom + verticalOffset, control.height))
        }

        Timer
        {
            id: showTimer
            running: target.selectedText && !draggingHandles && menuEnabled
            interval: 250
            onTriggered:
            {
                if (!menu.visible)
                {
                    menu.popup()
                    menu.adjust()
                }
            }
        }

        MenuItem
        {
            text: qsTr("Cut")
            enabled: target.selectedText.length > 0
            onTriggered: target.cut()
            focusPolicy: Qt.NoFocus
        }

        MenuItem
        {
            text: qsTr("Copy")
            enabled: target.selectedText.length > 0
            onTriggered: target.copy()
            focusPolicy: Qt.NoFocus
        }

        MenuItem
        {
            text: qsTr("Paste")
            onTriggered: target.paste()
            focusPolicy: Qt.NoFocus
        }

        MenuItem
        {
            text: qsTr("Select All")
            enabled: target.selectedText.length < target.text.length
            focusPolicy: Qt.NoFocus
            onTriggered: selectText(0, target.text.length)
        }
    }
}
