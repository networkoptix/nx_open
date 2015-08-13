import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

TextField
{
    id: thisComponent;

    property bool revertOnEmpty: false;

    property bool clearOnInitValue: false;

    placeholderText: initialText;

    property bool changed: changedBase;
    property bool changedBase: ((text !== initialText)
        && (!clearOnInitValue || (text.length || wasOnceChanged)));

    property string initialText: "";
    property bool wasOnceChanged: false;

    onTextChanged:
    {
        if (clearOnInitValue && (text != "") && (text != initialText))
            wasOnceChanged = true;
    }

    onChangedChanged:
    {
        if (changed)
            placeholderText = "";
    }

    activeFocusOnTab: true;
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
            if (clearOnInitValue && !changedBase)
                text = "";
            else
                selectAll();
        }
        else if ((revertOnEmpty || (clearOnInitValue && !wasOnceChanged)) && (text.length === 0))
        {
            text = initialText;
        }
    }
}

