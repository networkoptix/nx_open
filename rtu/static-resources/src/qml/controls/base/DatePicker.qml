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

    property bool changed: false;    
    property var changesHandler;
    
    property var date;
    property var initDate;
    
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 3;
    opacity: enabled ? 1.0 : 0.5;

    text: (enabled ? impl.stringFromDate(initDate) : "<now>");

    onEnabledChanged:
    {
        inputMask = (enabled ? impl.mask : "");
        text = (enabled ? impl.stringFromDate(date) : qsTr("<now>"));
    }

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
        
        if (initDate && date && (impl.stringFromDate(initDate) !== impl.stringFromDate(date)))
            thisComponent.changed = true;
    }
    
    property QtObject impl: QtObject
    {
        readonly property string dateFormat: Qt.locale().dateFormat(Locale.NarrowFormat);
        readonly property string mask: (dateFormat.replace(new RegExp(/[a-zA-Z0-9]/g), "9") + ";-");
        readonly property string emptyMaskValue: ":";  
    
        function stringFromDate(dateValue)
        {
            return (dateValue ? dateValue.toLocaleDateString(Qt.locale(), dateFormat) : "");
        }
    }
}

