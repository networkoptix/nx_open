import QtQuick 2.4;
import QtQuick.Controls 1.3 as QtControls;
import QtQuick.Controls.Styles 1.3;

import ".." as Common;
import "../standard" as Rtu;

QtControls.TextField
{
    id: thisComponent;

    function setTime(newTime)
    {
        text = impl.stringFromTime(newTime);
    }

    property bool changed: false;    
    property var changesHandler;
    
    property var time;
    property var initTime;
    
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 2;
    
    text: impl.stringFromTime(initTime);
    
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
        time = Date.fromLocaleTimeString(Qt.locale(), text, Locale.ShortFormat);
        if (!initTime && (text === impl.emptyMaskValue))
            return;
        
        if (initTime && time && (impl.stringFromTime(initTime) !== impl.stringFromTime(time)))
        {
            console.log("date time: " + initTime + ":" + time + ":" + (initTime === time));
            thisComponent.changed = true;
        }
    }
    
    property QtObject impl: QtObject
    {
        readonly property string timeFormat: "hh:mm";
        readonly property string mask: (timeFormat.replace(new RegExp(/[a-zA-Z0-9]/g), "9") + ";-");
        readonly property string emptyMaskValue: ":";  
    
        function stringFromTime(timeValue)
        {
            return (timeValue ? timeValue.toLocaleTimeString(Qt.locale(), timeFormat) : "");
        }
    }
}

