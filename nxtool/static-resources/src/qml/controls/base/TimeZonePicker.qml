import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "." as Base;
import "../../common" as Common;

ComboBox
{
    id: thisComponent;
    
    signal timeZoneChanged(int from, int to);
    
    property bool changed: (currentIndex !== initIndex);
    
    property int initIndex: init;
    property int lastSelectedIndex;
    property int fontSize: Common.SizeManager.fontSizes.base;
    
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 4;
    opacity: enabled ? 1.0 : 0.5;

    currentIndex: initIndex;
    
    textRole: "display";
    
    property bool firstTimeChange: true;
    
    onCurrentIndexChanged:
    {
        console.log("----------------- " + initIndex + ":" + currentIndex);
        if ((currentIndex == initIndex) && firstTimeChange)
        {
            firstTimeChange = false;
            lastSelectedIndex = currentIndex;
        }

        if (!model.isValidValue(currentIndex))
            return;
      
        if (currentIndex !== lastSelectedIndex)
            timeZoneChanged(lastSelectedIndex, currentIndex);

        lastSelectedIndex = currentIndex;
    }
    
    style: ComboBoxStyle
    {
        label: Base.Text
        {
            font.pixelSize: fontSize;
            text: thisComponent.currentText;
            verticalAlignment: Text.AlignVCenter;
        }
    }

    onTimeZoneChanged: console.log(from + ":" + to);
}
