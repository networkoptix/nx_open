import QtQuick 2.4
import QtQuick.Controls.Styles 1.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0

import "." as Base;
import "../../common" as Common;

FocusScope
{
    id: thisComponent;

    signal midnightCame(date newDate);

    function setTime(newTime, updateInitTime) { impl.setTime(newTime, updateInitTime); }

    property var time;
    property var initTime;
    property bool showNow: false;
    property bool acceptableInput: (time && !isNaN(time.getDate()));
    property bool changed: (hoursInput.changed || minutesInput.changed
        || secondsInput.changed || apInput.changed);

    width: row.width + Common.SizeManager.spacing.base;
    height: Common.SizeManager.clickableSizes.medium;

    opacity: (enabled ? 1: 0.5);

    Component.onCompleted: { setTime(initTime); }

    onInitTimeChanged: { impl.updateFromTime(initTime.toLocaleTimeString(), true); }

    onActiveFocusChanged:
    {
        if (!activeFocus)
        {
            hoursInput.focus = true;
        }
        else if (!hoursInput.activeFocus && !minutesInput.activeFocus
            && !secondsInput.activeFocus && !apInput.activeFocus)
        {
            hoursInput.forceActiveFocus();
        }
    }

    Timer
    {
        id: updateSecondsTimer;

        interval: 1000;
        running: !changed;
        repeat: true;
        onTriggered: impl.tick()
    }

    Rectangle
    {
        anchors.fill: parent;
        anchors.bottomMargin: -1;
        color: "#44ffffff";
        radius: baseRect.radius;
    }

    Rectangle
    {
        id: baseRect;

        radius: hoursInput.contentHeight * 0.16;
        anchors.fill: parent;
        border.color: thisComponent.activeFocus ? "#47b" : "#999";

        gradient: Gradient
        {
            GradientStop { color: "#e0e0e0" ; position: 0; }
            GradientStop { color: "#fff" ; position: 0.1; }
            GradientStop { color: "#fff" ; position: 1; }
        }
    }

    Row
    {
        id: row;
        spacing: Common.SizeManager.spacing.small;
        anchors.centerIn: parent;

        visible: !thisComponent.showNow;

        TimeSegment
        {
            id: hoursInput;

            formatNumber: impl.formatNumberHours;
            onTextChanged: { impl.updateTime(); }

            KeyNavigation.tab: minutesInput;
            KeyNavigation.right: KeyNavigation.tab;

            validator: IntValidator
            {
                bottom: (impl.useApDisplay ? 1 : 0);
                top: (impl.useApDisplay ? 12 : 23);
            }
        }

        Base.Text { text: ":"; }

        TimeSegment
        {
            id: minutesInput;

            formatNumber: impl.formatNumber;
            onTextChanged: { impl.updateTime(); }

            KeyNavigation.tab: secondsInput;
            KeyNavigation.backtab: hoursInput;

            validator: IntValidator
            {
                bottom: 0;
                top: 59;
            }
        }

        Base.Text { text: ":"; }

        TimeSegment
        {
            id: secondsInput

            formatNumber: impl.formatNumber;
            onTextChanged: { impl.updateTime(); }

            KeyNavigation.tab: (impl.useApDisplay ? apInput : null);
            KeyNavigation.backtab: minutesInput;

            validator: IntValidator
            {
                bottom: 0;
                top: 59;
            }
        }

        Base.Text
        {
            text: " ";
            visible: impl.useApDisplay;
        }

        TimeSegment
        {
            id: apInput;

            onTextChanged: { impl.updateTime(); }

            digital: false;
            visible: impl.useApDisplay;

            KeyNavigation.backtab: secondsInput;
            KeyNavigation.left: KeyNavigation.backtab;

            validator: RegExpValidator
            {
                regExp: /[AP]M/i;
            }
        }
    }

    Base.TextField
    {
        anchors.fill: parent;
        visible: thisComponent.showNow;
        initialText: "<now>";
    }

    QtObject
    {
        id: impl;

        readonly property string wrongDigitValue: "--";
        property var locale: Qt.locale();
        property bool useApDisplay: (locale.timeFormat(Locale.ShortFormat).indexOf("AP") != -1);

        function setTime(newTime, updateInitTime)
        {
            var oldChanged = changed;

            time = newTime;
            impl.updateFromTime(time.toLocaleTimeString(), false);

            if (updateInitTime && !oldChanged)
                initTime = time;
        }

        function tick()
        {
            var newInitTime = initTime;
            newInitTime.setSeconds(initTime.getSeconds() + 1);

            if (changed)
            {
                var newTime = time;
                newTime.setSeconds(time.getSeconds() + 1);
                thisComponent.setTime(newTime, false);
            }
            else
            {
                thisComponent.setTime(newInitTime, false);
            }

            initTime = newInitTime;
        }

        function getTimeString()
        {
            var result = formatNumberHours(hoursInput.text)
                + ":" + formatNumber(minutesInput.text)
                + ":" + formatNumber(secondsInput.text);

            if (useApDisplay)
                result += " " + apInput.text.toUpperCase();

            return result;
        }

        function updateTime()
        {
            time = Date.fromLocaleTimeString(Qt.locale(), getTimeString());
        }

        function setWrongTime(isInit)
        {
            setTimeInternal(wrongDigitValue, wrongDigitValue
                , wrongDigitValue, wrongDigitValue, isInit);
        }

        function setTimeInternal(hours, minutes, seconds, ampm, isInit)
        {
            if (isInit)
            {
                hoursInput.initialText = hours;
                minutesInput.initialText = minutes;
                secondsInput.initialText = seconds;
                if (useApDisplay)
                    apInput.initialText = ampm;
            }
            else
            {
                hoursInput.text = hours;
                minutesInput.text = minutes;
                secondsInput.text = seconds;
                if (useApDisplay)
                    apInput.text = ampm;
            }
        }

        function updateFromTime(timeValue, isInit)
        {
            var timeUpperCase = timeValue.toUpperCase()
            var re = /^(\d{1,2}):(\d{1,2})(?::(\d{1,2}))?\s*([AP]M)?/
            var match = re.exec(timeUpperCase);

            if (match == null)
            {
                setWrongTime(isInit);
                return;
            }

            var hours = parseInt(match[1])
            var minutes = parseInt(match[2])
            var seconds = parseInt(match[3])
            var ap = match[4]

            if (((hours > 23) || (minutes > 59) || (seconds > 59))
                || ((ap == "AM") && (hours > 12)))
            {
                setWrongTime();
                return;
            }

            if (!useApDisplay)
            {
                if (ap == "PM" && hours <= 12)
                    hours = (hours == 12 ? 0 : hours + 12);

            }
            else if (!ap)
            {

                if (hours == 0)
                {
                    hours = 12;
                    ap = "PM";
                }
                else if (hours > 12)
                {
                    hours -= 12;
                    ap = "PM";
                }
                else
                {
                    ap = "AM";
                }
            }

            setTimeInternal(formatNumberHours(hours), formatNumber(minutes)
                , formatNumber(seconds), ap, isInit);
        }

        function formatNumberHours(text)
        {
            var result = formatNumber(text);
            if ((result == "00") && impl.useApDisplay)
                result = "12";
            return result;
        }

        function formatNumber(text)
        {
            if (text == wrongDigitValue)
                return text;

            var number = parseInt(text)
            return (isNaN(number) ? "00" : (number < 10 ? "0" + number : number));
        }
    }
}
