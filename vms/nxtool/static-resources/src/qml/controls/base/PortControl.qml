import QtQuick 2.0;

import "." as Base;

Base.TextField
{
    id: thisComponent;

    property int initialPort: 0;
    
    property string lastValidValue: (initialPort ? initialPort : emptyPortString);
    readonly property string emptyPortString: "<different>";

    initialText: (initialPort ? initialPort.toString() : emptyPortString);

    horizontalAlignment: TextInput.AlignRight
    
    onActiveFocusChanged: 
    {
        if (activeFocus)
        {
            if (text === emptyPortString)
                initialText = "";
        }
        else if (text.length === 0)
        {
            initialText = (initialPort == 0 ? emptyPortString : initialPort.toString())
            text = lastValidValue;
        }
    }

    validator: IntValidator
    {
        bottom: 1;
        top: 65536;
    }

    onTextChanged:
    {
        if ((text !== emptyPortString) && (text.length > 0))
            lastValidValue = text;
    }

    Component.onCompleted:
    {
        if (!initialPort)
            placeholderText = emptyPortString;
    }
}

