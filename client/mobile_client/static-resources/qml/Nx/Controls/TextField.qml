import QtQuick 2.6
import QtQuick.Window 2.2
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
        visible: control.displayText === ""
        elide: Text.ElideRight
    }

    QtObject
    {
        id: d

        property bool selectionHandlesVisible: false

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

        function updateSelectionHandles()
        {
            if (!mobileMode)
                return

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

    QnScenePositionListener
    {
        id: positionListener
        item: control
        enabled: cursorHandle.visible ||
                 selectionStartHandle.visible ||
                 selectionEndHandle.visible
    }

    InputHandle
    {
        id: cursorHandle
        anchor: "cursorPosition"
        input: control
        x: positionListener.scenePos.x + localX
        y: positionListener.scenePos.y + localY
        parent: control.activeFocus ? Window.contentItem : control
    }

    InputHandle
    {
        id: selectionStartHandle
        anchor: "selectionStart"
        input: control
        x: positionListener.scenePos.x + localX
        y: positionListener.scenePos.y + localY
        parent: control.activeFocus ? Window.contentItem : control
    }

    InputHandle
    {
        id: selectionEndHandle
        anchor: "selectionEnd"
        input: control
        x: positionListener.scenePos.x + localX
        y: positionListener.scenePos.y + localY
        parent: control.activeFocus ? Window.contentItem : control
    }

    onPressAndHold:
    {
        contextMenu.x = pos.x
        contextMenu.y = pos.y

        control.persistentSelection = true
        contextMenu.open()
        control.persistentSelection = false
    }

    Menu
    {
        id: contextMenu

        /**
         * TODO: figure out why height is not calculated correctly
         * TODO: figure out why invisible MenuItem does not disapper from
         * menu and draws blank space instead
         */
        height: 4 * cutItem.height

        MenuItem
        {
            id: cutItem
            text: qsTr("Cut")
            enabled: control.selectedText.length
            onTriggered: { control.cut() }
        }

        MenuItem
        {
            text: qsTr("Copy")
            enabled: control.selectedText.length
            onTriggered: { control.copy() }
        }

        MenuItem
        {
            text: qsTr("Paste")
            enabled: control.canPaste
            onTriggered: { control.paste() }
        }

        MenuItem
        {
            text: qsTr("Select All")
            enabled: (control.selectedText != control.text)
            onTriggered: { control.selectAll() }
        }
    }

    Component.onCompleted:
    {
        if (Qt.platform.os == "android")
            passwordCharacter = "\u2022"
    }
}
