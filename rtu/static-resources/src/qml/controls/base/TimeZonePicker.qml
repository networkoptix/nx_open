import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../../common" as Common;

ComboBox
{
    id: thisComponent;
    
    signal timeZoneChanged(string from, string to);
    
    property bool changed: false;
    property var changesHandler;
    
    property int initIndex: init;
    property int lastSelectedIndex;
    
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 3;
    opacity: enabled ? 1.0 : 0.5;

    currentIndex: initIndex;
    
    textRole: "display";
    
    onChangedChanged: 
    {
        if (changesHandler)
            changesHandler.changed = true;
    }
    
    property bool firstTimeChange: true;
    
    onCurrentIndexChanged:
    {
        if ((currentIndex == initIndex) && firstTimeChange)
        {
            firstTimeChange = false;
            lastSelectedIndex = currentIndex;
        }

        if (changesHandler && (currentIndex !== initIndex))
        {
            thisComponent.changed = true;
            console.log("changed: " + currentIndex + ":" + initIndex)
        }
        console.log(currentIndex + ":" + initIndex)

        if (!changed)
            return;
        
        var newTimeZone = textAt(currentIndex);
        var newTimeZoneString = (newTimeZone ? newTimeZone : "");
        
        var lastTimeZone = textAt(lastSelectedIndex);
        var lastTimeZoneString = (lastTimeZone ? lastTimeZone : "");
        
        if (newTimeZoneString !== lastTimeZoneString)
            timeZoneChanged(lastTimeZoneString, newTimeZoneString);

        lastSelectedIndex = currentIndex;
    }
    
}
