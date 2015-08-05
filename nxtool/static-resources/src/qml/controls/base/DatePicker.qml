import QtQuick 2.1;
import QtQuick.Controls 1.1 as QtControls;
import QtQuick.Controls.Styles 1.1;

import "../../common" as Common;

QtControls.TextField
{
    id: thisComponent;

    function setDate(newTime)
    {
        text = impl.stringFromDate(newTime);
    }

    property bool showNow: false;
    property bool changed: false;    
    property int fontSize: Common.SizeManager.fontSizes.base;
    
    property var date;
    property var initDate;
    
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 3;
    
    opacity: enabled ? 1.0 : 0.5;
    
    text: impl.stringFromDate(initDate);
    inputMask: impl.mask;

    onShowNowChanged:
    {
        inputMask = (showNow ? "" : impl.mask);
        text = (showNow ? qsTr("<now>") : impl.stringFromDate(initDate));
    }

    font.pixelSize: fontSize;
    
    style: TextFieldStyle
    {
        renderType: Text.NativeRendering;
    }

    onTextChanged: 
    {
        date = Date.fromLocaleDateString(Qt.locale(), text, impl.dateFormat);
        if (!initDate && (text === impl.emptyMaskValue))
            return;
        
        thisComponent.changed = initDate && date && (impl.stringFromDate(initDate) !== impl.stringFromDate(date));
    }

    validator: RegExpValidator
    {
        regExp: /^(((0[1-9]|[12]\d|3[01])\/(0[13578]|1[02])\/((1[6-9]|[2-9]\d)\d{2}))|((0[1-9]|[12]\d|30)\/(0[13456789]|1[012])\/((1[6-9]|[2-9]\d)\d{2}))|((0[1-9]|1\d|2[0-8])\/02\/((1[6-9]|[2-9]\d)\d{2}))|(29\/02\/((1[6-9]|[2-9]\d)(0[48]|[2468][048]|[13579][26])|((16|[2468][048]|[3579][26])00))))$/
     //   regExp: /^(?:(?:(?:0[1-9]|1\d|2[0-8])(?:0[1-9]|1[0-2])|(?:29|30)(?:0[13-9]|1[0-2])|31(?:0[13578]|1[02]))[1-9]\d{3}|2902(?:[1-9]\d(?:0[48]|[2468][048]|[13579][26])|(?:[2468][048]|[13579][26])00))$/
    }
   
    property QtObject impl: QtObject
    {
        readonly property string dateFormat: "dd/MM/yyyy";
        readonly property string mask: (dateFormat.replace(new RegExp(/[a-zA-Z0-9]/g), "9") + ";-");
        readonly property string emptyMaskValue: ":";  
    
        function stringFromDate(dateValue)
        {
            return (dateValue ? dateValue.toLocaleDateString(Qt.locale(), dateFormat) : "");
        }
    }
}

