import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

TextField
{
    id: thisComponent;

    property bool changed: (text !== initialText);
    
    property string initialText: "";

    implicitHeight: Common.SizeManager.clickableSizes.medium;
    implicitWidth: implicitHeight * 4;

    text: initialText;
    font.pointSize: Common.SizeManager.fontSizes.medium;
    
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
            selectAll();
    }
}

