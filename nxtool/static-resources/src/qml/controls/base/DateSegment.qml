import QtQuick 2.4;

import "../../common" as Common;

TextInput
{
    id: thisComponent;

    property var formatFunction;

    property string fallbackText;
    property var separators: [".", "/", "-", " "];

    width: implicitHeight;
    renderType: Text.NativeRendering;
    font.pixelSize: Common.SizeManager.fontSizes.base;

    KeyNavigation.right: KeyNavigation.tab;
    KeyNavigation.left: KeyNavigation.backtab;

    maximumLength: 2;
    horizontalAlignment: TextInput.AlignHCenter;

    QtObject
    {
        id: impl;

        function testDigit(key)
        {
            return ((key >= Qt.Key_0) && (key <= Qt.Key_9));
        }
    }

    Keys.onReleased:
    {
        if (impl.testDigit(event.key) && (cursorPosition == 2) && (selectionStart == selectionEnd)
                && KeyNavigation.tab)
        {
            KeyNavigation.tab.forceActiveFocus();
        }
    }

    Keys.onPressed:
    {
        if (event.key == Qt.Key_Backspace)
        {
            if (KeyNavigation.backtab && (cursorPosition == 0))
            {
                KeyNavigation.backtab.forceActiveFocus();
                event.accepted = true;
            }
        }
        else if ((event.key == Qt.Key_Up) || (event.key == Qt.Key_Down))
        {
            var value = parseInt(text);
            if (isNaN(value))
                return;

            value += (event.key == Qt.Key_Up ? 1 : -1);
            if (validator)
                value = Math.max(Math.min(value, validator.top), validator.bottom);

            text = (formatFunction ? formatFunction(value) : value);
        }
        else if (validator && (validator.bottom > 0) && (event.key == Qt.Key_0)
            && (text.length == maximumLength - 1) && (parseInt(text) == 0))
        {
            event.accepted = true;
            return;
        }
        else if ((cursorPosition == text.length) && (separators.indexOf(event.text) != -1)
            && KeyNavigation.tab)
        {
            KeyNavigation.tab.forceActiveFocus()
        }
    }

    onActiveFocusChanged:
    {
        var newText = (formatFunction ? formatFunction(text) : text)
        if (!activeFocus && (newText != text))
            text = newText;
    }

    onCursorVisibleChanged:
    {
        if (cursorVisible)
        {
            selectAll();
        }
        else if (validator)
        {
            var value = parseInt(text);
            if (value < validator.bottom || value > validator.top || isNaN(value))
            {
                text = fallbackText;
            }
        }
    }
}

