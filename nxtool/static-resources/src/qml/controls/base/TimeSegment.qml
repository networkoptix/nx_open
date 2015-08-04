import QtQuick 2.4

import "../../common" as Common;

TextInput
{
    id: timeSegment

    property var formatNumber;
    property bool digital: true;
    property bool changed: (text != initialText);

    property string initialText: "";

    text: initialText;
    selectByMouse: true;

    width: implicitHeight * 1;
    renderType: Text.NativeRendering;
    font.pixelSize: Common.SizeManager.fontSizes.base;


    KeyNavigation.right: KeyNavigation.tab;
    KeyNavigation.left: KeyNavigation.backtab;

    maximumLength: 2;
    horizontalAlignment: TextInput.AlignHCenter;

    onCursorVisibleChanged:
    {
        if (cursorVisible)
        {
            selectAll();
        }
        else
        {
            if (digital)
                text = formatNumber(text);
            else
                text = (text.toUpperCase().indexOf('P') != -1 ? "PM" : "AM");
        }
    }

    Keys.onReleased:
    {
        if (event.key == Qt.Key_Backspace)
        {
            if (text == "")
            {
                text = (digital ? (formatNumber("00")) : "AM");
                if (KeyNavigation.backtab)
                    KeyNavigation.backtab.forceActiveFocus();
                else
                    selectAll();
            }
        }
        else if (digital && (event.key >= Qt.Key_0) && (event.key < Qt.Key_9)
            && (cursorPosition == 2) && (selectionStart == selectionEnd) && KeyNavigation.tab)
        {
            KeyNavigation.tab.forceActiveFocus();
        }
    }

    Keys.onPressed:
    {
        if ((digital && validator.bottom > 0) && (event.key == Qt.Key_0) && (text == "0"))
        {
            event.accepted = true;
            return;
        }

        if ((cursorPosition == text.length) && KeyNavigation.tab
            && ((event.key == Qt.Key_Colon) || (event.key == Qt.Key_Space)))
        {
            KeyNavigation.tab.forceActiveFocus();
        }
    }

    onActiveFocusChanged:
    {
        if (activeFocus)
            return;

        initialText = formatNumber(initialText);
        text = formatNumber(text);
    }
}

