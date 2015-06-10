import QtQuick 2.1;
import QtQuick.Controls 1.1;

TextField
{
    id: thisComponent;

    signal ipAddressChanged;
    property alias correct: thisComponent.acceptableInput;

    focus: true;
    inputMask: "000.000.000.000; "
    validator: RegExpValidator
    {
        regExp: /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
    }

    onActiveFocusChanged:
    {
        if (activeFocus)
        {
            selectAll();
        }
        else if (!acceptableInput)
        {
//            cursorPosition = 1;
            selectAll();
//            forceActiveFocus();
        }


    }

    onEditingFinished:
    {
        if (!acceptableInput)
            selectAll();
    }
}

