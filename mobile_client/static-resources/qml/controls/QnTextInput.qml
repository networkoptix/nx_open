import QtQuick 2.4

import "../main.js" as Main

FocusScope {
    id: root

    property alias acceptableInput: textInput.acceptableInput
    property alias activeFocusOnPress: textInput.activeFocusOnPress
    property alias canPaste: textInput.canPaste
    property alias canRedo: textInput.canRedo
    property alias canUndo: textInput.canUndo
    property alias color: textInput.color
    property alias cursorPosition: textInput.cursorPosition
    property alias cursorRectangle: textInput.cursorRectangle
    property alias cursorVisible: textInput.cursorVisible
    property alias displayText: textInput.displayText
    property alias echoMode: textInput.echoMode
    property alias effectiveHorizontalAlignment: textInput.effectiveHorizontalAlignment
    property alias font: textInput.font
    property alias horizontalAlignment: textInput.horizontalAlignment
    property alias inputMask: textInput.inputMask
    property alias inputMethodComposing: textInput.inputMethodComposing
    property alias inputMethodHints: textInput.inputMethodHints
    property alias length: textInput.length
    property alias maximumLength: textInput.maximumLength
    property alias passwordCharacter: textInput.passwordCharacter
    property alias passwordMaskDelay: textInput.passwordMaskDelay
    property alias persistentSelection: textInput.persistentSelection
    property alias readOnly: textInput.readOnly
    property alias renderType: textInput.renderType
    property alias selectedText: textInput.selectedText
    property alias selectedTextColor: textInput.selectedTextColor
    property alias selectionColor: textInput.selectionColor
    property alias selectionEnd: textInput.selectionEnd
    property alias selectionStart: textInput.selectionStart
    property alias text: textInput.text
    property alias validator: textInput.validator
    property alias verticalAlignment: textInput.verticalAlignment
    property alias wrapMode: textInput.wrapMode

    property color cursorColor: color
    property bool selectionAllowed: true

    implicitWidth: textInput.implicitWidth
    implicitHeight: textInput.implicitHeight

    signal accepted()
    signal editingFinished()

    function copy() {
        textInput.copy()
    }

    function cut() {
        textInput.cut()
    }

    function deselect() {
        textInput.deselect()
    }

    function ensureVisible(position) {
        d.ensureVisible(position)
    }

    function getText(start, end) {
        return textInput.getText(start, end)
    }

    function insert(position, text) {
        textInput.insert(position, text)
    }

    function isRightToLeft(start, end) {
        return textInput.isRightToLeft(start, end)
    }

    function moveCursorSelection(position, mode) {
        textInput.moveCursorSelection(position, mode)
    }

    function paste() {
        textInput.paste()
    }

    function positionAt(x, y, position) {
        return textInput.positionAt(flickable.contentX + x, y, position)
    }

    function positionToRectangle(pos) {
        return textInput.positionToRectangle(pos)
    }

    function redo() {
        textInput.redo()
    }

    function remove(start, end) {
        return textInput.remove(start, end)
    }

    function select(start, end) {
        return textInput.select(start, end)
    }

    function selectAll() {
        textInput.selectAll()
    }

    function selectWord() {
        textInput.selectWord()
    }

    function undo() {
        textInput.undo()
    }

    onSelectionAllowedChanged: {
        if (!selectionAllowed)
            deselect()
    }

    QtObject {
        id: d

        property bool selectionHandlesVisible: false

        function showCursorHandle() {
            cursorHandle.opacity = 0.0
            cursorHandle.visible = true
            cursorHandle.opacity = 1.0
        }

        function hideCursorHandle() {
            cursorHandle.opacity = 0.0
            cursorHandle.visible = false
        }

        function updateSelectionHandles() {
            if (cursorHandle.pressed)
                return

            if (textInput.selectionStart == textInput.selectionEnd) {
                selectionHandlesVisible = false
            } else {
                hideCursorHandle()
                selectionHandlesVisible = true
            }
        }

        onSelectionHandlesVisibleChanged: {
            if (selectionHandlesVisible) {
                selectionStartHandle.opacity = 0.0
                selectionEndHandle.opacity = 0.0
                selectionStartHandle.visible = true
                selectionEndHandle.visible = true
                selectionEndHandle.opacity = 1.0
                selectionStartHandle.opacity = 1.0
            } else {
                selectionStartHandle.opacity = 0.0
                selectionEndHandle.opacity = 0.0
                selectionStartHandle.visible = false
                selectionEndHandle.visible = false
            }
        }

        function ensureAnchorVisible(anchor) {
            ensureVisible(textInput[anchor])
        }

        function ensureVisible(position) {
            var rect = textInput.positionToRectangle(position)
            if (rect.x < flickable.contentX)
                flickable.contentX = rect.x
            var right = rect.x + rect.width
            if (right > flickable.contentX + flickable.width)
                flickable.contentX = right - flickable.width
        }
    }

    Flickable {
        id: flickable

        anchors.fill: parent

        flickableDirection: Flickable.HorizontalFlick

        contentWidth: textInput.width
        contentHeight: textInput.height
        interactive: contentWidth - 4 > width

        clip: true
        boundsBehavior: Flickable.StopAtBounds

        TextInput {
            id: textInput

            height: root.height

            focus: true
            selectByMouse: !Main.isMobile()

            verticalAlignment: Text.AlignVCenter
            clip: false

            autoScroll: false

            onCursorPositionChanged: d.ensureAnchorVisible("cursorPosition")

            onActiveFocusChanged: {
                if (!activeFocus)
                    d.hideCursorHandle()
            }

            onTextChanged: d.hideCursorHandle()

            onSelectionStartChanged: {
                if (selectionStart == selectionEnd - 1 && !selectionEndHandle.insideInputBounds)
                    d.ensureAnchorVisible("selectionEnd")
                else
                    d.ensureAnchorVisible("selectionStart")
                d.updateSelectionHandles()
            }
            onSelectionEndChanged: {
                if (selectionEnd == selectionStart + 1 && !selectionStartHandle.insideInputBounds)
                    d.ensureAnchorVisible("selectionStart")
                else
                    d.ensureAnchorVisible("selectionEnd")
                d.updateSelectionHandles()
            }

            onEditingFinished: root.editingFinished()
            onAccepted: root.accepted()

            cursorDelegate: Rectangle {
                id: cursor
                width: dp(1)
                color: cursorColor

                Timer {
                    interval: 500
                    running: textInput.cursorVisible && !cursorHandle.pressed
                    repeat: true
                    onTriggered: cursor.visible = !cursor.visible
                    onRunningChanged: cursor.visible = running || cursorHandle.pressed
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent

        cursorShape: Qt.IBeamCursor

        onClicked: {
            var pos = textInput.positionAt(textInput.x + flickable.contentX + mouse.x, mouse.y)
            textInput.cursorPosition = pos

            if (!textInput.activeFocus) {
                textInput.forceActiveFocus()
            } else {
                d.showCursorHandle()
            }
        }

        onPressAndHold: {
            textInput.forceActiveFocus()
            if (mouse.x > textInput.width) {
                // TODO: context menu
                return
            }

            if (!selectionAllowed)
                return

            var pos = textInput.positionAt(textInput.x + flickable.contentX + mouse.x, mouse.y)
            textInput.cursorPosition = pos
            textInput.selectWord()
        }
    }


    QnInputHandle {
        id: cursorHandle
        anchor: "cursorPosition"
        input: textInput
        flickable: flickable
        visible: false
    }

    QnInputHandle {
        id: selectionStartHandle
        anchor: "selectionStart"
        input: textInput
        flickable: flickable
        visible: false
    }

    QnInputHandle {
        id: selectionEndHandle
        anchor: "selectionEnd"
        input: textInput
        flickable: flickable
        visible: false
    }
}

