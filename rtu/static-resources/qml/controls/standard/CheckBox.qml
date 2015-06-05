import QtQuick 2.4;
import QtQuick.Controls 1.3;
import QtQuick.Controls.Styles 1.3;

import ".." as Common;

CheckBox
{
    id: thisComponent;

    property bool changed: false;
        
    property var changesHandler;
    property int initialCheckedState: Qt.Checked;
    property int fontSize: Common.SizeManager.fontSizes.medium;

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
        if (changesHandler && (checkedState != initialCheckedState))
        {
            thisComponent.changesHandler.changed = true;
            thisComponent.changed = true;
        }
    }
}

