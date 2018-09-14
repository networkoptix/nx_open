import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Controls 2.4
import com.networkoptix.qml 1.0
import Nx 1.0
import Nx.Controls 1.0

import "private"

TextInput
{
    id: control

    property bool showError: false

    property bool mobileMode: Utils.isMobile()

    property color inactiveColor: showError ? ColorTheme.red_d1 : ColorTheme.base10
    property color activeColor: showError ? ColorTheme.red_main : ColorTheme.brand_main
    property color placeholderColor: ColorTheme.base10
    property color cursorColor: activeColor
    property bool selectionAllowed: true
    scrollByMouse: mobileMode
    selectByMouse: !mobileMode
    color: showError ? ColorTheme.red_main : ColorTheme.windowText

    leftPadding: 8
    rightPadding: 8
    height: 48
    verticalAlignment: TextInput.AlignVCenter

    font.pixelSize: 16

    background: Item
    {
        Rectangle
        {
            width: parent.width
            anchors
            {
                margins: 4
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }
            height: 1
            color: control.activeFocus ? activeColor : inactiveColor
        }

        Rectangle
        {
            anchors
            {
                left: parent.left
                leftMargin: 4
                bottom: parent.bottom
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
                right: parent.right
                rightMargin: 4
                bottom: parent.bottom
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
            if (!mobileMode)
                return

            cursorHandle.opacity = 0.0
            cursorHandle.visible = true
            cursorHandle.opacity = 1.0
        }

        function hideCursorHandle()
        {
            if (!mobileMode)
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
            if (!mobileMode)
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
            var pos = positionAt(event.x, event.y)
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

            openContextMenu()
        }

        function openContextMenu()
        {
            persistentSelection = true
            needRestoreContextMenu = false
            contextMenu.open()
            contextMenu.adjustPosition()
            persistentSelection = false
        }
    }

    onClicked:
    {
        if (event.button === Qt.RightButton)
        {
            d.selectWordAndOpenContextMenu(event, false)
            return
        }

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

    QnScenePositionListener
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
        parent: Window.contentItem
    }

    InputHandle
    {
        id: selectionStartHandle
        anchor: "selectionStart"
        input: control
        x: positionListener.scenePos.x + localX
        y: positionListener.scenePos.y + localY
        parent: Window.contentItem

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
        parent: Window.contentItem

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
            var yOffset = cursorRectangle.height + (mobileMode ? 28 : 4)
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
    }

    Component.onCompleted:
    {
        if (Qt.platform.os == "android")
            passwordCharacter = "\u2022"
    }
}
