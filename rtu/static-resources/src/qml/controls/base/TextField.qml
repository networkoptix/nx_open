import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

TextField
{
    id: thisComponent;

    property bool changed: false;
    
    property string initialText: "";
    property var changesHandler;
    
    implicitHeight: Common.SizeManager.clickableSizes.medium;
    implicitWidth: implicitHeight * 4;

    text: initialText;
    font.pointSize: Common.SizeManager.fontSizes.medium;
    
    style: TextFieldStyle
    {
        renderType: Text.NativeRendering;
    }

    onTextChanged: 
    {
        if (changesHandler && (text !== initialText))
        {
            thisComponent.changesHandler.changed = true;
            thisComponent.changed = true;
        }
    }
    
}

