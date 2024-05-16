// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Templates as T

import Nx.Core

import nx.vms.client.desktop

import "private"

T.TextField
{
    id: control

    property bool warningState: false
    placeholderTextColor: ColorTheme.transparent(color, 0.5)
    property bool hidePlaceholderOnFocus: false
    property bool selectPlaceholderOnFocus: false
    property bool contextMenuOpening: false

    property int prevSelectionStart: 0
    property int prevSelectionEnd: 0
    property int prevCursorPosition: -1

    property var cursorShape: Qt.IBeamCursor

    property Component menu: Component
    {
        EditContextMenu
        {
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            readActionsVisible: control.echoMode == TextInput.Normal
            editingEnabled: control.enabled && !control.readOnly
            onCutAction: control.cut()
            onCopyAction: control.copy()
            onPasteAction: control.paste()
            onDeleteAction: control.remove(control.selectionStart, control.selectionEnd);
            onItemActiveFocusChanged: (itemActiveFocus) =>
            {
                if (itemActiveFocus)
                    control.restoreSelection()
            }

            onAboutToHide:
            {
                if (control.focus && control.visible)
                    control.forceActiveFocus()
            }
        }
    }

    function restoreSelection()
    {
        if (control.prevCursorPosition > control.prevSelectionStart)
            select(control.prevSelectionStart, control.prevSelectionEnd)
        else
            select(control.prevSelectionEnd, control.prevSelectionStart)
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

    implicitWidth: 200
    implicitHeight: 28

    // baselineOffset of the TextField is properly calculated only when width and height properties
    // have some default bindings (QQuickTextInputPrivate::updateBaselineOffset).
    width: implicitWidth
    height: implicitHeight

    leftPadding: 8
    rightPadding: 8

    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: 14
    passwordCharacter: "\u25cf"
    selectByMouse: true
    color: enabled ? ColorTheme.text : ColorTheme.transparent(ColorTheme.text, 0.3)
    selectionColor: ColorTheme.highlight
    selectedTextColor: ColorTheme.brightText

    background: TextFieldBackground
    {
        control: parent
    }

    PlaceholderText
    {
        anchors
        {
            fill: control
            leftMargin: control.leftPadding
            rightMargin: control.rightPadding
            topMargin: control.topPadding
            bottomMargin: control.bottomPadding
        }
        verticalAlignment: control.verticalAlignment

        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        renderType: control.renderType
        elide: Text.ElideRight

        visible: !control.length && !control.preeditText
            && (!control.activeFocus
                || (!control.hidePlaceholderOnFocus
                    && control.horizontalAlignment !== Qt.AlignHCenter))

        Rectangle
        {
            width: parent.contentWidth
            height: parent.height

            z: -1
            color: control.selectionColor
            visible: control.selectPlaceholderOnFocus && control.activeFocus
        }
    }

    ContextMenuMouseArea
    {
        id: menuMouseArea

        anchors.fill: control
        anchors.topMargin: control.topPadding
        anchors.bottomMargin: control.bottomPadding
        anchors.leftMargin: control.leftPadding
        anchors.rightMargin: control.rightPadding

        menu: control.menu
        cursorShape: control.cursorShape
        parentSelectionStart: control.selectionStart
        parentSelectionEnd: control.selectionEnd
        parentCursorPosition: control.cursorPosition

        CursorOverride.shape: cursorShape
        CursorOverride.active: containsMouse
        hoverEnabled: true

        onMenuAboutToBeOpened:
        {
            contextMenuOpening = true
            control.forceActiveFocus()
        }

        onMenuOpened: (prevSelectionStart, prevSelectionEnd, prevCursorPosition) =>
        {
            contextMenuOpening = false
            control.prevSelectionStart = prevSelectionStart
            control.prevSelectionEnd = prevSelectionEnd
            control.prevCursorPosition = prevCursorPosition
            control.restoreSelection()
        }
    }
}
