import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

TextField
{
    id: thisComponent;

    property bool revertOnEmpty: false;
    property bool changed: (text !== initialText);
    
    property string initialText: "";

    implicitHeight: Common.SizeManager.clickableSizes.medium;
    implicitWidth: implicitHeight * 4;

    opacity: (enabled ? 1 : 0.5);
    text: initialText;
    font.pixelSize: Common.SizeManager.fontSizes.base;
    
    Binding
    {
        target: thisComponent;
        property: "text";
        value: initialText;
    }
   
    style: TextFieldStyle
    {
        renderType: Text.NativeRendering;
    }

    onActiveFocusChanged: 
    {
        if (activeFocus)
        {
            selectAll();
        }
        else if (revertOnEmpty && text.length === 0)
        {
            text = initialText;
        }
    }
}

