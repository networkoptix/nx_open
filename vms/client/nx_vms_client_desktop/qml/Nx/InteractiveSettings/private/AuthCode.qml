// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core
import Nx.Controls

import nx.vms.client.core

import "../../Controls/private"

FocusScope
{
    id: control

    property int count: 4
    property var format: /[\d\w]/g
    property bool warningState: false
    readonly property bool valid: value.length === count
    readonly property string value:
    {
        let result = ""
        for (let i = 0; i < items.count; ++i)
            result += items.itemAt(i).text

        return result
    }

    signal edited()

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight
    baselineOffset: layout.baselineOffset

    function clear()
    {
        for (let i = 0; i < items.count; ++i)
            items.itemAt(i).text = ""

        warningState = false
        resetFocus()
    }

    function paste(text)
    {
        const code = text.match(format)

        if (!code)
            return

        for (let i = 0; i < items.count; ++i)
        {
            items.itemAt(i).text = code[i] ?? ""
            items.itemAt(i).focus = false
        }
    }

    function resetFocus()
    {
        if (items.count > 0)
            items.itemAt(0).focus = true
    }

    component Field: TextField
    {
        readonly property color borderColor: background.border.color

        implicitWidth: 28
        implicitHeight: 28
        leftPadding: 6
        rightPadding: 6
        placeholderText: "0"
        horizontalAlignment: TextInput.AlignHCenter
        validator: RegularExpressionValidator { regularExpression: format }
        font: FontConfig.textEdit
        warningState: control.warningState

        background: TextFieldBackground
        {
            control: parent
            radius: 2
        }

        function previous()
        {
            if (index - 1 >= 0)
                items.itemAt(index - 1).forceActiveFocus()
        }

        function next()
        {
            if (index + 1 <= count - 1)
                nextItemInFocusChain().forceActiveFocus()
            else
                items.itemAt(index).focus = false
        }

        Keys.onRightPressed: next()
        Keys.onLeftPressed: previous()
        Keys.onPressed: (event) =>
        {
            if (event.key === Qt.Key_Backspace)
                previous()

            if (event.matches(StandardKey.Paste))
            {
                event.accepted = true
                control.paste(NxGlobals.clipboardText())
            }
        }

        onActiveFocusChanged:
        {
            if (activeFocus)
                selectAll()
            else
                deselect()
        }

        onTextEdited:
        {
            const isFilled = text.length === 1
            if (isFilled)
                next()

            control.edited()
        }

        onSelectedTextChanged:
        {
            if (activeFocus && text)
                selectAll()
        }
    }

    Column
    {
        id: layout

        spacing: 4

        Row
        {
            spacing: 8

            Repeater
            {
                id: items
                model: control.count
                Field { }
            }
        }

        Text
        {
            text: qsTr("Wrong pairing code")
            font: FontConfig.textEdit
            color: items.count > 0 ? items.itemAt(0).borderColor : ColorTheme.windowText
            visible: control.warningState
        }
    }

    ContextMenuMouseArea
    {
        anchors.fill: parent

        menu: Menu
        {
            id: menu

            property bool selectionActionsEnabled: false // Required by ContextMenuMouseArea.

            Action
            {
                text: qsTr("Paste")
                shortcut: StandardKey.Paste
                onTriggered: control.paste(NxGlobals.clipboardText())
                enabled: menu.visible
            }
        }
    }

    Keys.onPressed: (event) =>
    {
        if (event.matches(StandardKey.Paste))
        {
            event.accepted = true
            control.paste(NxGlobals.clipboardText())
        }
    }

    Component.onCompleted: resetFocus()
}
