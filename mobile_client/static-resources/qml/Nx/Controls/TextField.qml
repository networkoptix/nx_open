import QtQuick 2.6
import com.networkoptix.qml 1.0
import Nx 1.0

import "private"

QnTextInput
{
    id: control

    property bool showError: false

    property color inactiveColor: showError ? ColorTheme.orange_d1 : ColorTheme.base10
    property color activeColor: showError ? ColorTheme.orange_main : ColorTheme.brand_main
    property color placeholderColor: ColorTheme.base10
    property color cursorColor: activeColor
    property bool selectionAllowed: true
    color: showError ? ColorTheme.orange_main : ColorTheme.windowText

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
            onRunningChanged: cursor.visible = running || cursorHandle.pressed
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
        visible: !control.displayText && !control.inputMethodComposing
        elide: Text.ElideRight
    }

    QtObject
    {
        id: d

        property bool selectionHandlesVisible: false

        function showCursorHandle()
        {
            cursorHandle.opacity = 0.0
            cursorHandle.visible = true
            cursorHandle.opacity = 1.0
        }

        function hideCursorHandle()
        {
            cursorHandle.opacity = 0.0
            cursorHandle.visible = false
        }

        function updateSelectionHandles()
        {
            if (cursorHandle.pressed)
                return

            console.log(control.selectionStart, control.selectionEnd, control.selectionStart == control.selectionEnd)

            if (control.selectionStart == control.selectionEnd)
            {
                selectionHandlesVisible = false
            }
            else
            {
                hideCursorHandle()
                selectionHandlesVisible = true
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
    }

    onClicked:
    {
        if (displayText)
            d.showCursorHandle()
    }

    onActiveFocusChanged:
    {
        if (!activeFocus)
            d.hideCursorHandle()
    }

    onTextChanged: d.hideCursorHandle()

    // TODO: #dklychkov Finish implementation
//    onSelectionStartChanged:
//    {
//        if (selectionStart == selectionEnd - 1 && !selectionEndHandle.insideInputBounds)
//            d.ensureAnchorVisible("selectionEnd")
//        else
//            d.ensureAnchorVisible("selectionStart")
//        d.updateSelectionHandles()
//    }
//    onSelectionEndChanged:
//    {
//        if (selectionEnd == selectionStart + 1 && !selectionStartHandle.insideInputBounds)
//            d.ensureAnchorVisible("selectionStart")
//        else
//            d.ensureAnchorVisible("selectionEnd")
//        d.updateSelectionHandles()
//    }

    InputHandle
    {
        id: cursorHandle
        anchor: "cursorPosition"
        input: control
    }

    InputHandle
    {
        id: selectionStartHandle
        anchor: "selectionStart"
    }

    InputHandle
    {
        id: selectionEndHandle
        anchor: "selectionEnd"
    }
}
