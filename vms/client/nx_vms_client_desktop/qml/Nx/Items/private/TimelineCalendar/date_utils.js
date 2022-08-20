// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function areDatesEqual(date1, date2)
{
    return date1.getFullYear() === date2.getFullYear()
        && date1.getMonth() === date2.getMonth()
        && date1.getDate() === date2.getDate()
}

function addDays(date, days)
{
    return new Date(date.getFullYear(), date.getMonth(), date.getDate() + days)
}

function addMilliseconds(date, milliseconds)
{
    return new Date(date.getTime() + milliseconds)
}

function stripTime(date)
{
    return new Date(date.getFullYear(), date.getMonth(), date.getDate())
}
