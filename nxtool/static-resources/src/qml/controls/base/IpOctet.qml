import QtQuick 2.0

import "../../common" as Common;

TextInput
{
    id: thisComponent;

    property var nextOctet;
    property var prevOctet;

    property string initialText: "";
    property bool changed: (text != initialText);

    text: initialText;

    width: implicitHeight * 1.3;

    selectByMouse: true;
    opacity: (enabled ? 1 : 0.5);
    renderType: Text.NativeRendering;
    font.pixelSize: Common.SizeManager.fontSizes.base;

    horizontalAlignment: Qt.AlignHCenter;

    Binding
    {
        target: thisComponent;
        property: "text";
        value: initialText;
    }

    validator: IntValidator
    {
        top: 255;
        bottom: 0;
    }

    KeyNavigation.backtab: (prevOctet ? prevOctet : null);
    KeyNavigation.tab: (nextOctet ? nextOctet : null);
    KeyNavigation.left: (prevOctet ? prevOctet : null);
    KeyNavigation.right: (nextOctet ? nextOctet : null);

    Keys.onPressed:
    {
        if (text.length && (event.key === Qt.Key_0) && !cursorPosition)
        {
            event.accepted = true;
        }
        else if ((event.key === Qt.Key_Backspace) && !length)
        {
            impl.moveToPrevOctet();
        }
        else if ((event.text === '.') && (cursorPosition == length))
        {
            if (!text.length)
                text = "0";
            impl.moveToNextOctet();
        }
    }

    onTextChanged:
    {
        var kMaxDigitsInoctet = 3;
        if (activeFocus && (length == kMaxDigitsInoctet))
            impl.moveToNextOctet();
    }

    onActiveFocusChanged:
    {
        if (activeFocus)
            selectAll();
    }

    property QtObject impl: QtObject
    {
        function moveToNextOctet()
        {
            if (nextOctet)
                nextOctet.forceActiveFocus();
        }

        function moveToPrevOctet()
        {
            if (prevOctet)
                prevOctet.forceActiveFocus();
        }
    }
}

