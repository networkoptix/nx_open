import QtQuick 2.1;
import QtQuick.Controls 1.1 as QtControls;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

QtControls.TextField
{
    id: thisComponent;

    function setTime(newTime)
    {
        text = impl.stringFromTime(newTime);
    }

    property bool showNow: false;
    property bool changed: false;    
    property int fontSize: Common.SizeManager.fontSizes.base;
    
    property var time;
    property var initTime;
    
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 3;

    opacity: enabled ? 1.0 : 0.5;

    text: impl.stringFromTime(initTime);
    inputMask: impl.mask;

    enabled: !showNow;
    
    onShowNowChanged:
    {
        inputMask = (showNow ? "" : impl.mask);
        text = (showNow ? qsTr("<now>") : impl.stringFromTime(initTime));
    }

    font.pixelSize: fontSize;
    
    style: TextFieldStyle
    {
        renderType: Text.NativeRendering;
    }

    validator: RegExpValidator
    {
        regExp: /([01]?[0-9]|2[0-3]):[0-5][0-9]/;
    }

    onTextChanged: 
    {
        time = Date.fromLocaleTimeString(Qt.locale(), text, impl.timeFormat);
        if (!initTime && (text === impl.emptyMaskValue))
            return;
        
        thisComponent.changed = (initTime && time 
            && (impl.stringFromTime(initTime) !== impl.stringFromTime(time)));
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

