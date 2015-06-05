import QtQuick 2.4;
import QtQuick.Controls 1.3 as QtControls;
import QtQuick.Controls.Styles 1.3;

import ".." as Common;
import "../standard" as Rtu;

QtControls.TextField
{
    id: thisComponent;

    function setDate(newTime)
    {
        text = impl.stringFromDate(newTime);
    }

    property bool changed: false;    
    property var changesHandler;
    
    property var date;
    property var initDate;
    
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 3;
    
    text: impl.stringFromDate(initDate);
    
    font.pointSize: Common.SizeManager.fontSizes.medium;
    
    style: TextFieldStyle
    {
        renderType: Text.NativeRendering;
    }

    inputMask: impl.mask;
    
    onChangedChanged: 
    {
        if (changesHandler)
            changesHandler.changed = true;
    }

    onTextChanged: 
    {
        date = Date.fromLocaleDateString(Qt.locale(), text, Locale.ShortFormat);
        if (!initDate && (text === impl.emptyMaskValue))
            return;
        
        if (initDate && date && (initDate !== date))
            thisComponent.changed = true;
    }
    
    property QtObject impl: QtObject
    {
        readonly property string dateFormat: Qt.locale().dateFormat(Locale.NarrowFormat);
        readonly property string mask: (dateFormat.replace(new RegExp(/[a-zA-Z0-9]/g), "9") + ";-");
        readonly property string emptyMaskValue: ":";  
    
        function stringFromDate(timeValue)
        {
            return (timeValue ? timeValue.toLocaleDateString(Qt.locale(), dateFormat) : "");
        }
    }
}

