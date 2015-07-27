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

    Keys.onBacktabPressed:
    {
        impl.moveToPrevOctet();
    }

    Keys.onTabPressed:
    {
        impl.moveToNextOctet();
    }

    Keys.onRightPressed:
    {
        if (cursorPosition === length)
            impl.moveToNextOctet();
        else
            ++cursorPosition;
    }

    Keys.onLeftPressed:
    {
        if (!cursorPosition)
            impl.moveToPrevOctet();
        else
            --cursorPosition;
    }

    Keys.onPressed:
    {
        if ((event.key === Qt.Key_Backspace) && !length)
            impl.moveToPrevOctet();
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

