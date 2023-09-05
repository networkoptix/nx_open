// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

FocusScope
{
    id: control

    property alias text: edit.text
    property alias readOnly: edit.readOnly
    property alias wrapMode: edit.wrapMode

    property var cursorShape: Qt.IBeamCursor

    Flickable
    {
        id: flickable

        boundsBehavior: Flickable.StopAtBounds
        clip: true
        anchors.fill: parent

        ScrollBar.vertical: ScrollBar
        {
            id: scrollBar

            width: 6
            parent: flickable.parent

            anchors
            {
                right: flickable.right
                rightMargin: 1
                top: flickable.top
                topMargin: 1
                bottom: flickable.bottom
                bottomMargin: 1
            }
        }

        TextArea.flickable: TextArea
        {
            id: edit

            focus: true

            wrapMode: TextEdit.Wrap
            cursorPosition: 0
            onTextChanged:
            {
                // When long text is set make sure it is scrolled to the top.
                if (cursorPosition == 0)
                    scrollTimer.start()
            }

            Timer
            {
                id: scrollTimer

                interval: 0
                onTriggered: edit.cursorPosition = 0
            }
        }
    }

    property Component menu: Component
    {
        EditContextMenu
        {
            editingEnabled: control.enabled && !control.readOnly
            onCutAction: edit.cut()
            onCopyAction: edit.copy()
            onPasteAction: edit.paste()
            onDeleteAction: edit.remove(edit.selectionStart, edit.selectionEnd)
            onAboutToHide:
            {
                if (control.focus && control.visible)
                    edit.forceActiveFocus()
            }
        }
    }

    function closeMenu()
    {
        if (menuMouseArea.menuItem)
            menuMouseArea.menuItem.close()
    }

    onVisibleChanged:
    {
        closeMenu()
        if (!visible)
            focus = false
    }

    ContextMenuMouseArea
    {
        id: menuMouseArea

        anchors.fill: control
        anchors.rightMargin: scrollBar.visible
            ? scrollBar.width + scrollBar.anchors.rightMargin
            : 0

        menu: control.menu
        cursorShape: control.cursorShape
        parentSelectionStart: edit.selectionStart
        parentSelectionEnd: edit.selectionEnd

        CursorOverride.shape: control.cursorShape
        CursorOverride.active: containsMouse
        hoverEnabled: true

        onMenuAboutToBeOpened: edit.forceActiveFocus()
    }
}
