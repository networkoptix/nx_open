// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Templates 2.15 as T

import Nx 1.0

import nx.vms.client.desktop 1.0

import "private"

T.TextField
{
    id: control

    property bool warningState: false
    placeholderTextColor: ColorTheme.transparent(color, 0.5)
    property bool hidePlaceholderOnFocus: false

    property int prevSelectionStart: 0
    property int prevSelectionEnd: 0

    property var cursorShape: Qt.IBeamCursor

    property Component menu: Component
    {
        EditContextMenu
        {
            readActionsVisible: control.echoMode == TextInput.Normal
            onCutAction: control.cut()
            onCopyAction: control.copy()
            onPasteAction: control.paste()
            onDeleteAction: control.remove(control.selectionStart, control.selectionEnd);
            onItemActiveFocusChanged:
            {
                if (itemActiveFocus)
                    control.restoreSelection()
            }
        }
    }

    function restoreSelection()
    {
        select(control.prevSelectionStart, control.prevSelectionEnd)
        focus = true
    }

    implicitWidth: 200
    implicitHeight: 28

    padding: 6
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
        id: placeholder

        anchors.fill: control
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding
        topPadding: control.topPadding
        bottomPadding: control.bottomPadding

        text: control.placeholderText
        font: control.font
        color: placeholderTextColor
        horizontalAlignment: control.horizontalAlignment
        verticalAlignment: control.verticalAlignment
        renderType: control.renderType
        elide: Text.ElideRight

        visible: !control.length && !control.preeditText
            && (!control.activeFocus
                || (!control.hidePlaceholderOnFocus
                    && control.horizontalAlignment !== Qt.AlignHCenter))
    }

    ContextMenuMouseArea
    {
        anchors.fill: control
        anchors.topMargin: control.topPadding
        anchors.bottomMargin: control.bottomPadding
        anchors.leftMargin: control.leftPadding
        anchors.rightMargin: control.rightPadding

        menu: control.menu
        cursorShape: control.cursorShape
        parentSelectionStart: control.selectionStart
        parentSelectionEnd: control.selectionEnd

        CursorOverride.shape: cursorShape
        CursorOverride.active: containsMouse
        hoverEnabled: true

        onMenuOpened:
        {
            control.prevSelectionStart = prevSelectionStart
            control.prevSelectionEnd = prevSelectionEnd
            control.restoreSelection()
        }
    }
}
