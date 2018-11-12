import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

CheckBox
{
    id: thisComponent;

    property bool changed: checkedState != initialCheckedState;
        
    property int initialCheckedState: Qt.Checked;
    property int fontSize: Common.SizeManager.fontSizes.base;

    opacity: enabled ? 1.0 : 0.5;
    checkedState: initialCheckedState;

    style: CheckBoxStyle
    {
        id: checkboxStyle;

        label: Text
        {
            id: checkboxLabel;

            font.pixelSize: thisComponent.fontSize;
            text: thisComponent.text;
            renderType: Text.NativeRendering;
        }
    }
}

