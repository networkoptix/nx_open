// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls

Control
{
    id: control

    property int count: 4
    property var format: /[\d\w]/g
    readonly property bool valid: value.length === count
    readonly property string value:
    {
        let result = ""
        for (let i = 0; i < items.count; ++i)
            result += items.itemAt(i).text

        return result
    }

    function clear()
    {
        for (let i = 0; i < items.count; ++i)
            items.itemAt(i).text = ""

        resetFocus()
    }

    function paste(text)
    {
        const code = text.match(format)
        if (code.length !== count)
            return clear()

        for (let i = 0; i < items.count; ++i)
            items.itemAt(i).text = code[i]
    }

    function resetFocus()
    {
        if (items.count > 0)
            items.itemAt(0).focus = true
    }

    component Field: TextField
    {
        implicitWidth: 20
        implicitHeight: 28
        leftPadding: 6
        rightPadding: 6
        placeholderText: "0"
        validator: RegularExpressionValidator { regularExpression: format }

        function previous()
        {
            if (index - 1 >= 0)
                items.itemAt(index - 1).forceActiveFocus()
        }

        function next()
        {
            if (index + 1 <= count - 1)
                items.itemAt(index + 1).forceActiveFocus()
        }

        Keys.onRightPressed: (event) =>
        {
            const cursorAtEnd = cursorPosition === 1
            if (cursorAtEnd)
                next()
            else
                event.accepted = false
        }

        Keys.onLeftPressed: (event) =>
        {
            const cursorAtStart = cursorPosition === 0
            if (cursorAtStart)
                previous()
            else
                event.accepted = false
        }

        Keys.onPressed: (event) =>
        {
            if (event.matches(StandardKey.Paste))
            {
                event.accepted = true
                control.paste(NxGlobals.clipboardText())
            }
        }

        onTextEdited:
        {
            const isFilled = text.length === 1
            if (isFilled)
                next()
        }
    }

    contentItem: Row
    {
        spacing: 8

        Repeater
        {
            id: items
            model: control.count
            Field { }
        }
    }

    Component.onCompleted: resetFocus()
}
