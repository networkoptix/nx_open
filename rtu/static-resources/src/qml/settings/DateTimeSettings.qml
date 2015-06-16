import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/rtu" as Rtu;
import "../controls/expandable" as Expandable;

import networkoptix.rtu 1.0 as Utils;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;
    
    changed:  (maskedArea && maskedArea.changed?  true : false);
    
    function tryApplyChanges()
    {
        if (!changed)
            return true;
        
        return maskedArea.flaggedItem.currentItem.tryApplyChanges();
    }

    propertiesGroupName: qsTr("Set Device Date & Time");
    propertiesDelegate: Item
    {
        property bool changed: (!flagged.showFirst ? false 
            : flagged.currentItem.changed);
    
        property alias flaggedItem: flagged;
    
        width: flagged.width + flagged.anchors.leftMargin;
        height: flagged.height;
        
        Rtu.FlaggedItem
        {
            id: flagged;
                
            message: qsTr("Can not change date and time for some selected servers");
            showItem: ((Utils.Constants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
                || (rtuContext.selection.count === 1));
            
            anchors
            {
                left: parent.left;
                top: parent.top;
                leftMargin: Common.SizeManager.spacing.base;
            }

            item : Component
            {
                id: dateTimePage;
                
                Grid
                {
                    id: dateTimeGrid;
                 
                    function tryApplyChanges()
                    {
                        if (!timeZonePicker.model.isValidValue(timeZonePicker.currentIndex))
                        {
                            timeZonePicker.focus = true;
                            return false;
                        }
                        
                        if (!useCurrentTimeCheckbox.checked && !timePicker.acceptableInput)
                        {
                            timeZonePicker.focus = true;
                            return false;
                        }
                        
                        if (!useCurrentTimeCheckbox.checked && !datePicker.acceptableInput)
                        {
                            datePicker.focus = true;
                            return false;
                        }
                        
                        var newDate = datePicker.date;
                        var newTime = timePicker.time;
                        if (useCurrentTimeCheckbox.checked)
                        {
                            if (useCurrentTimeCheckbox.checked)
                            {
                                var now = new Date();
                                newDate = now;
                                newTime = now;
                            }
                        }
                        rtuContext.changesManager().addDateTimeChange(
                            newDate, newTime, timeZonePicker.currentText);
                        return true;
                    }

                    property bool changed: (timeZonePicker.changed || timePicker.changed || datePicker.changed);
                    enabled: (Utils.Constants.AllowChangeDateTimeFlag & rtuContext.selection.flags);
    
                    verticalItemAlignment: Grid.AlignVCenter;
                    
                    spacing: Common.SizeManager.spacing.base;
                    anchors
                    {
                        left: (parent ? parent.left : undefined);
                        right: (parent ? parent.right : undefined);
                        top: (parent ? parent.top : undefined);
                    }
    
                    columns: 4;
    
                    Base.Text
                    {
                        text: qsTr("Time zone");
                    }
                    
                    Base.Text
                    {
                        text: qsTr("Date");
                    }
    
                    Base.Text
                    {
                        text: qsTr("Time");
                    }
    
                    Item
                    {
                        width: 1;
                        height: 1;
                    }
    
                    Base.TimeZonePicker
                    {
                        id: timeZonePicker;
                        
                        model: rtuContext.timeZonesModel(this);
                        initIndex: timeZonePicker.model.initIndex;
    
                        onTimeZoneChanged:
                        {
                            if (useCurrentTimeCheckbox.checked)
                                return;
                            
                            var dateTime = rtuContext.applyTimeZone(datePicker.date, timePicker.time, from, to);
                            datePicker.setDate(dateTime);
                            timePicker.setTime(dateTime);
                        }
                    }

                    Base.DatePicker
                    {
                        id: datePicker;
                        
                        initDate: (rtuContext.selection && rtuContext.selection !== null ?
                            rtuContext.selection.dateTime : undefined);
                    }
                    Base.TimePicker
                    {
                        id: timePicker;
                        
                        initTime: (rtuContext.selection && rtuContext.selection !== null ?
                            rtuContext.selection.dateTime : undefined);
                    }
                    
                    Base.CheckBox
                    {
                        id: useCurrentTimeCheckbox;
        
                        text: qsTr("Set current date/time");
                        initialCheckedState: Qt.Unchecked;
                        
                        onCheckedChanged: 
                        {
                            if (checked)
                                timeZonePicker.currentIndex = timeZonePicker.model.currentTimeZoneIndex;
                            
                            datePicker.showNow = checked;
                            timePicker.showNow = checked;
                        }
                    }
                }
            }
        }
    }   
}

