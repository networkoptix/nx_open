import QtQuick 2.4;

import ".."
import "../standard" as Rtu;

MaskedSettingsPanel
{
    id: thisComponent;

    onApplyButtonPressed: 
    {
        
    }

    propertiesGroupName: qsTr("Date and time");
    propertiesDelegate: Component
    {
        id: dateTimePage;

        Item
        {
            height: dateTimeGrid.height + dateTimeGrid.anchors.topMargin;

            Grid
            {
                id: dateTimeGrid;

                spacing: SizeManager.spacing.base;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    top: parent.top;

                    leftMargin: SizeManager.spacing.base;
                    topMargin: SizeManager.spacing.base;
                }

                columns: 4;

                Rtu.Text
                {
                    text: qsTr("Date");
                }

                Rtu.Text
                {
                    text: qsTr("Time");
                }

                Rtu.Text
                {
                    text: qsTr("Time zone");
                }

                Item
                {
                    width: 1;
                    height: 1;
                }
  
                Rtu.DatePicker
                {
                    id: datePicker;
                    
                    changesHandler: thisComponent;
                    initDate: rtuContext.selectionDateTime;
                }

                
                Rtu.TimePicker
                {
                    id: timePicker;
                    
                    changesHandler: thisComponent;
                    initTime: rtuContext.selectionDateTime;
                }

                Rtu.TimeZonePicker 
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
/*
                    onCurrentIndexChanged:
                    {
                        if (!changed)
                            return;
                        var newIndex = model.onCurrentIndexChanged(currentIndex);
                        currentIndex = newIndex;

                        
                        var newDateTime = rtuContext.applyTimeZone(datePicker.date, timePicker.time
                            , currentText);
                        timePicker.setTime(newDateTime);
                        datePicker.setDate(newDateTime);
                    }
                    */
                }
/*
                Rtu.CheckBox
                {
                    changesHandler: thisComponent;

                    text: qsTr("Set current date/time");
                    initialCheckedState: Qt.Unchecked;
                    
                    onCheckedChanged: 
                    {
                        if (checked)
                            datePicker.date = new Date();
                    }
                }
                */
            }
        }
    }
}

