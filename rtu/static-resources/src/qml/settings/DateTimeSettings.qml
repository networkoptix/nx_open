import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;

    propertiesGroupName: qsTr("Date and time");
    propertiesDelegate: Component
    {
        id: dateTimePage;

        Item
        {
            height: dateTimeGrid.height + dateTimeGrid.anchors.topMargin;
            
            Connections
            {
                target: thisComponent;
                
                onApplyButtonPressed:
                {
                    if (timeZonePicker.changed || datePicker.changed || timePicker.changed
                        || useCurrentTimeCheckbox.changed)
                    {
                        var newDate = datePicker.date;
                        var newTime = timePicker.time;
                        if (useCurrentTimeCheckbox.checked)
                        {
                            var now = new Date();
                            newDate = now;
                            newTime = now;
                        }

                        rtuContext.changesManager().addDateTimeChange(
                            newDate, newTime, timeZonePicker.currentText);
                    }
                }
            }
            
            Grid
            {
                id: dateTimeGrid;

                spacing: Common.SizeManager.spacing.base;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    top: parent.top;

                    leftMargin: Common.SizeManager.spacing.base;
                    topMargin: Common.SizeManager.spacing.base;
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
                    changesHandler: thisComponent;    
                    
                    
                    model: rtuContext.timeZonesModel(this);
                    initIndex: timeZonePicker.model.initIndex;

                    onTimeZoneChanged:
                    {
                        var dateTime = rtuContext.applyTimeZone(datePicker.date, timePicker.time, from, to);
                        datePicker.setDate(dateTime);
                        timePicker.setTime(dateTime);
                    }
                }

                Base.DatePicker
                {
                    id: datePicker;
                    
                    changesHandler: thisComponent;
                    initDate: (rtuContext.selection && rtuContext.selection !== null ?
                        rtuContext.selection.dateTime : undefined);
                }
                
                Base.TimePicker
                {
                    id: timePicker;
                    
                    changesHandler: thisComponent;
                    initTime: (rtuContext.selection && rtuContext.selection !== null ?
                        rtuContext.selection.dateTime : undefined);
                }

                Base.CheckBox
                {
                    id: useCurrentTimeCheckbox;

                    changesHandler: thisComponent;

                    text: qsTr("Set current date/time");
                    initialCheckedState: Qt.Unchecked;
                    

                    onCheckedChanged: 
                    {
                        if (checked)
                        {
                            timeZonePicker.currentIndex = timeZonePicker.model.currentTimeZoneIndex;
                        }
                        datePicker.enabled = !checked;
                        timePicker.enabled = !checked;
                    }
                }
                
            }
        }
    }
}

