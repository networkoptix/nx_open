import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/rtu" as Rtu;
import "../controls/expandable" as Expandable;
import "../dialogs" as Dialogs;

import networkoptix.rtu 1.0 as NxRtu;

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
    propertiesDelegate: FocusScope
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
            showItem: ((NxRtu.Constants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
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
                            timePicker.focus = true;
                            return false;
                        }
                        
                        if (!useCurrentTimeCheckbox.checked && !datePicker.acceptableInput)
                        {
                            datePicker.focus = true;
                            return false;
                        }
                        
                        var newDate = datePicker.date;
                        var newTime = timePicker.time;
                        var timeZoneId = timeZonePicker.model.timeZoneIdByIndex(timeZonePicker.currentIndex)

                        if (useCurrentTimeCheckbox.checked)
                        {
                            var now = new Date();
                            var nowConverted = rtuContext.applyTimeZone(now, now
                                , ""    /// Use empty time zone to point to current zone id
                                , timeZoneId);
                            newDate = nowConverted;
                            newTime = nowConverted;
                        }

                        rtuContext.changesManager().addDateTimeChange(newDate, newTime, timeZoneId);
                        return true;
                    }

                    property bool changed: (timeZonePicker.changed || timePicker.changed
                        || datePicker.changed || useCurrentTimeCheckbox.changed);
                    enabled: (NxRtu.Constants.AllowChangeDateTimeFlag & rtuContext.selection.flags);
    
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
                        thin: false;
                        text: qsTr("Time zone");
                    }
                    
                    Base.Text
                    {
                        thin: false;
                        text: qsTr("Date");
                    }

                    Base.Text
                    {
                        thin: false;
                        text: qsTr("Time");
                    }
    
                    Base.EmptyCell {}
    
                    Base.TimeZonePicker
                    {
                        id: timeZonePicker;
                        
                        model: rtuContext.timeZonesModel(this);
                        initIndex: timeZonePicker.model.initIndex;
    
                        onTimeZoneChanged:
                        {
                            if (useCurrentTimeCheckbox.checked)
                                return;
                            
                            var prevZoneId = timeZonePicker.model.timeZoneIdByIndex(from);
                            var curZoneId = timeZonePicker.model.timeZoneIdByIndex(to);
                            console.log("Chaning timezone from " + prevZoneId + " to " + curZoneId);
                            var dateTime = rtuContext.applyTimeZone(datePicker.date, timePicker.time, prevZoneId, curZoneId);
                            datePicker.setDate(dateTime);
                            timePicker.setTime(dateTime);
                        }
                    }

                    Row
                    {
                        spacing: Common.SizeManager.spacing.small;

                        Base.DatePicker
                        {
                            id: datePicker;

                            initDate: (rtuContext.selection && rtuContext.selection !== null ?
                                rtuContext.selection.dateTime : new Date());
                        }

                        Base.Button
                        {
                            width: datePicker.height;
                            height: datePicker.height;

                            enabled: !useCurrentTimeCheckbox.checked;

                            Dialogs.CalendarDialog
                            {
                                id: calendarDialog;

                                onDateChanged: { datePicker.setDate(newDate); }
                            }

                            onClicked:
                            {
                                calendarDialog.initDate = isNaN(datePicker.date) ? new Date() : datePicker.date;
                                calendarDialog.show();
                            }

                            Image
                            {
                                id: name
                                anchors.centerIn: parent;
                                source: "qrc:/resources/calendar.png";
                            }
                        }
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

