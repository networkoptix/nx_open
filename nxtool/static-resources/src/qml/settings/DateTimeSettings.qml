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

    property bool pseudoEnabled: enabled; /// Have to use this property to allow tooltips under time zone control

    changed:  (maskedArea && maskedArea.changed?  true : false);

    extraWarned: !((NxRtu.RestApiConstants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
        || (rtuContext.selection.count === 1)) || rtuContext.selection.safeMode;

    function tryApplyChanges(warnings)
    {
        if (!changed)
            return true;
        
        return maskedArea.flaggedItem.currentItem.tryApplyChanges();
    }

    onMaskedAreaChanged:
    {
        if (!warned || !maskedArea)
            return;

        var flagged = maskedArea.flaggedItem;
        if (!flagged || !flagged.currentItem || !flagged.currentItem.timeZonePickerControl)
            return;

        flagged.currentItem.timeZonePickerControl.forceActiveFocus();
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
            showItem: ((NxRtu.RestApiConstants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
                || (rtuContext.selection.count === 1));
            
            anchors
            {
                left: parent.left;
                top: parent.top;
                leftMargin: Common.SizeManager.spacing.base;
            }

            Dialogs.ErrorDialog
            {
                id: errorDialog;
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
                            errorDialog.message = "Please choose time zone";
                            errorDialog.show();
                            timeZonePicker.forceActiveFocus();

                            return false;
                        }
                        
                        if (!useCurrentTimeCheckbox.checked && !datePicker.acceptableInput)
                        {
                            errorDialog.message = "Please enter valid date";
                            errorDialog.show();

                            datePicker.forceActiveFocus();
                            return false;
                        }

                        if (!useCurrentTimeCheckbox.checked && !timePicker.acceptableInput)
                        {
                            errorDialog.message = "Please enter valid time";
                            errorDialog.show();

                            timePicker.forceActiveFocus();
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

                        rtuContext.changesManager().changeset().addDateTimeChange(newDate, newTime, timeZoneId);
                        return true;
                    }

                    property alias timeZonePickerControl: timeZonePicker;

                    property bool changed: (timeZonePicker.changed || timePicker.changed
                        || datePicker.changed || useCurrentTimeCheckbox.changed);
    
                    property bool dontConvertTimeZone: false;
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

                        pseudoEnabled: ((NxRtu.RestApiConstants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
                            && thisComponent.pseudoEnabled);

                        model: rtuContext.selection.timeZonesModel;
                        initIndex: timeZonePicker.model.initIndex;
    
                        onTimeZoneChanged:
                        {
                            if (dontConvertTimeZone || !timePicker.acceptableInput
                                    || !datePicker.acceptableInput)
                                return;
                            
                            var prevZoneId = timeZonePicker.model.timeZoneIdByIndex(from);
                            var curZoneId = timeZonePicker.model.timeZoneIdByIndex(to);

                            if (!timeZonePicker.model.isValidValue(from)
                                || !timeZonePicker.model.isValidValue(to))
                            {
                                return;
                            }

                            var dateTime = rtuContext.applyTimeZone(datePicker.date, timePicker.time, prevZoneId, curZoneId);
                            datePicker.setDate(dateTime);
                            timePicker.setTime(dateTime, true);
                        }

                        KeyNavigation.tab: datePicker;
                    }

                    Row
                    {
                        spacing: Common.SizeManager.spacing.small;

                        Base.DateEdit
                        {
                            id: datePicker;
                            initDate: (rtuContext.selection && rtuContext.selection !== null ?
                                rtuContext.selection.dateTime : new Date());

                            enabled: ((NxRtu.RestApiConstants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
                                && !useCurrentTimeCheckbox.checked && thisComponent.pseudoEnabled);

                            KeyNavigation.tab: selectDateButton;
                            KeyNavigation.backtab: timeZonePicker;
                        }

                        Base.Button
                        {
                            id: selectDateButton;

                            activeFocusOnTab: true;

                            width: datePicker.height;
                            height: datePicker.height;

                            enabled: (!useCurrentTimeCheckbox.checked
                                && (NxRtu.RestApiConstants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
                                && thisComponent.pseudoEnabled);

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

                                enabled: parent.enabled;
                                opacity: (enabled ? 1.0 : 0.5);
                            }

                            KeyNavigation.tab: timePicker;
                            KeyNavigation.backtab: datePicker;
                        }
                    }

                    Base.TimeEdit
                    {
                        id: timePicker;
                        
                        enabled: ((NxRtu.RestApiConstants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
                            && !useCurrentTimeCheckbox.checked && thisComponent.pseudoEnabled);
                        initTime: (rtuContext.selection && rtuContext.selection !== null ?
                            rtuContext.selection.dateTime : undefined);

                        KeyNavigation.tab: useCurrentTimeCheckbox;
                        KeyNavigation.backtab: selectDateButton;
                    }
                    
                    Base.CheckBox
                    {
                        id: useCurrentTimeCheckbox;
        
                        activeFocusOnTab: true;
                        activeFocusOnPress: true;

                        text: qsTr("Set current date/time");
                        initialCheckedState: Qt.Unchecked;
                        enabled: ((NxRtu.RestApiConstants.AllowChangeDateTimeFlag & rtuContext.selection.flags)
                            && thisComponent.pseudoEnabled);

                        onCheckedChanged: 
                        {
                            dateTimeGrid.dontConvertTimeZone = true;

                            if (checked)
                                timeZonePicker.currentIndex = timeZonePicker.model.currentTimeZoneIndex;
                            else
                                timeZonePicker.currentIndex = timeZonePicker.lastSelectedIndex;

                            datePicker.showNow = checked;
                            timePicker.showNow = checked;

                            dateTimeGrid.dontConvertTimeZone = false;
                        }
                    }
                }
            }
        }
    }   
}

