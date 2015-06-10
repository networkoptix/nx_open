import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

CheckBox
{
    id: thisComponent;

    property bool changed: false;
        
    property var changesHandler;
    property int initialCheckedState: Qt.Checked;
    property int fontSize: Common.SizeManager.fontSizes.medium;

    checkedState: initialCheckedState;

    style: CheckBoxStyle
    {
        id: checkboxStyle;

        label: Text
        {
            id: checkboxLabel;

            font.pointSize: thisComponent.fontSize;
            text: thisComponent.text;
            renderType: Text.NativeRendering;
        }
    }

    onCheckedStateChanged: 
    {
        if (checkedState != initialCheckedState)
        {
            if (changesHandler)
                changesHandler.changed = true;

            changed = true;
        }
    }
}

