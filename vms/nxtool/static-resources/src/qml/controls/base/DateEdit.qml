import QtQuick 2.4;
import QtQuick.Layouts 1.1;

import "../../common" as Common;
import "." as Base;

FocusScope
{
    id: thisComponent;

    function setDate(newDate) { impl.setDate(newDate); }

    readonly property bool changed: (impl.toMSeconds(date) !== impl.toMSeconds(initDate));
    readonly property bool acceptableInput: (impl.isCorrectDate(date))

    property var date;
    property var initDate;
    property bool showNow: false;

    opacity: (enabled ? 1 : 0.5);
    height: Common.SizeManager.clickableSizes.medium;
    width: height * 3;

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

        radius: dayInput.contentHeight * 0.16;
        anchors.fill: parent;
        border.color: thisComponent.activeFocus ? "#47b" : "#999";

        gradient: Gradient
        {
            GradientStop { color: "#e0e0e0" ; position: 0; }
            GradientStop { color: "#fff" ; position: 0.1; }
            GradientStop { color: "#fff" ; position: 1; }
        }
    }

    GridLayout
    {
        id: content;

        anchors.centerIn: parent;
        visible: !thisComponent.showNow;

        rows: 1;
        columns: 5;

        columnSpacing: Common.SizeManager.spacing.small;

        DateSegment
        {
            id: dayInput

            fallbackText: (impl.isCorrectDate(date) ?
                date.getDate() : impl.kWrongFieldValue);
            validator: IntValidator
            {
                bottom: 1
                top: 31
            }

            formatFunction: impl.formatField;
            onTextChanged: { impl.updateDate(); }
        }

        Base.Text
        {
            id: firstSeparator;
            Layout.column: 1;
        }

        DateSegment
        {
            id: monthInput;

            fallbackText: (impl.isCorrectDate(date) ?
                date.getMonth() + 1 : impl.kWrongFieldValue) ;
            validator: IntValidator
            {
                bottom: 1;
                top: 12;
            }

            formatFunction: impl.formatField;
            onTextChanged: { impl.updateDate(); }
        }

        Base.Text
        {
            id: secondSeparator;
            Layout.column: 3;
        }

        DateSegment
        {
            id: yearInput;

            maximumLength: 4;
            fallbackText: (impl.isCorrectDate(date) ?
                date.getFullYear() : impl.kYearWrongValue);
            validator: IntValidator
            {
                bottom: 2000;
                top: 2100;
            }

            formatFunction: impl.formatYear;
            onTextChanged: { impl.updateDate(); }
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

        readonly property var locale: Qt.locale();
        readonly property string format: impl.locale.dateFormat(Locale.ShortFormat);

        readonly property string kWrongFieldValue: "--";
        readonly property string kYearWrongValue: kWrongFieldValue + kWrongFieldValue;

        function setDate(newDate)
        {
            var tmpDate = impl.plainDate(newDate);
            var isCorrectDate = impl.isCorrectDate(tmpDate);

            yearInput.text = impl.formatYear(isCorrectDate ? tmpDate.getFullYear() : impl.kYearWrongValue);
            monthInput.text = impl.formatField(isCorrectDate ? tmpDate.getMonth() + 1 : impl.kWrongFieldValue);
            dayInput.text = impl.formatField(isCorrectDate ? tmpDate.getDate() : impl.kWrongFieldValue);

            date = tmpDate;
        }

        function formatField(text) { return formatNumber(text, impl.kWrongFieldValue); }

        function formatYear(text) { return formatNumber(text, impl.kYearWrongValue); }

        function formatNumber(text, wrongValue)
        {
            if (text == wrongValue)
                return text;

            var number = parseInt(text)
            return (isNaN(number) ? text : (number < 10 ? "0" + number : number));
        }

        function toMSeconds(newDate)
        {
            return (newDate && !isNaN(newDate.getTime()) ? newDate.getTime() : 0);
        }

        function isCorrectDate(testDate)
        {
            return (impl.toMSeconds(testDate) !== 0);
        }

        function daysInMonth(year, month)
        {
            return new Date(year, month + 1, 0).getDate();
        }

        function updateDate()
        {
            var acceptable = yearInput.text.length && monthInput.text.length && dayInput.text.length
                && yearInput.acceptableInput && monthInput.acceptableInput && dayInput.acceptableInput;
            var newDate = (acceptable ? new Date(yearInput.text, monthInput.text - 1, dayInput.text) : undefined);
            date = (isCorrectDate(newDate) ? newDate : undefined);
        }

        function plainDate(value)
        {
            return (!isCorrectDate(value) ? undefined
                : new Date(value.getFullYear(), value.getMonth(), value.getDate()));
        }

        onFormatChanged:
        {
            var re = /^(\w)\w*([^\w]+)(\w)\w*([^\w]+)(\w)/;
            var match = re.exec(impl.format.toUpperCase());

            if (!match)
                return;

            firstSeparator.text = match[2];
            secondSeparator.text = match[4];

            var dayIndex = match.indexOf("D") - 1;
            var monthIndex = match.indexOf("M") - 1;
            var yearIndex = match.indexOf("Y") - 1;

            if ((dayIndex < 0) || (monthIndex < 0) || (yearIndex < 0))
                return;

            dayInput.Layout.column = dayIndex;
            monthInput.Layout.column = monthIndex;
            yearInput.Layout.column = yearIndex;

            var items = [dayInput];
            if (monthIndex > dayIndex)
                items.push(monthInput);
            else
                items.unshift(monthInput);
            items.splice(yearIndex, 0, yearInput);

            for (var i = 0; i < items.length; ++i)
            {
                items[i].KeyNavigation.tab = (i < items.length - 1 ? items[i + 1] : null);
                items[i].KeyNavigation.backtab = (i > 0 ? items[i - 1] : null);
            }
        }

    }

    onInitDateChanged:
    {
        var plainDate = impl.plainDate(initDate);
        if (impl.toMSeconds(plainDate) !== impl.toMSeconds(initDate))
            initDate = plainDate;
        thisComponent.setDate(initDate);
    }

    onActiveFocusChanged:
    {
        if (activeFocus)
        {
            if (!dayInput.activeFocus && !monthInput.activeFocus && !yearInput.activeFocus)
                dayInput.forceActiveFocus();

            return;
        }

        var acceptable = yearInput.text.length && yearInput.acceptableInput
            && monthInput.text.length && monthInput.acceptableInput;
        if (!acceptable)
            return;
        var maxDays = impl.daysInMonth(yearInput.text, monthInput.text - 1);
        if (parseInt(dayInput.text) > maxDays)
            dayInput.text = maxDays;

        dayInput.focus = true;
    }
}

