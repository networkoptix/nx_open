// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx
import Nx.Core

import nx.vms.client.desktop

NxObject
{
    property date linkValidFrom
    property date linkValidUntil
    property int displayOffsetMs: 0
    property int expiresAfterLoginS: -1
    property bool revokeAccessEnabled: false
    property date firstLoginTime

    property var highlightColor: ColorTheme.colors.light10
    property var timeOffsetProvider

    readonly property bool isLoginTimeValid: !isNaN(firstLoginTime.getTime())

    // Returns linkValidUntil or firstLoginTime + expiresAfterLoginS, whichever comes first.
    readonly property date expirationDate:
    {
        let validUntilMs = linkValidUntil.getTime()

        if (revokeAccessEnabled && !isNaN(firstLoginTime.getTime()))
        {
            validUntilMs = Math.min(
                validUntilMs,
                firstLoginTime.getTime() + expiresAfterLoginS * 1000)
        }

        return new Date(validUntilMs)
    }

    readonly property string expirationDateText:
    {
        const time = expirationDate.getTime()
        const expirationDisplay = new Date(time + displayOffsetMs)

        const endOfDay = expirationDisplay.getHours() == 23
            && expirationDisplay.getMinutes() == 59
            && expirationDisplay.getSeconds() == 59

        return endOfDay
            ? NxGlobals.dateInShortFormat(expirationDisplay)
            : NxGlobals.dateTimeInShortFormat(expirationDisplay)
    }

    readonly property string validityDatesText:
    {
        const dateFrom = highlight(`${displayDate(linkValidFrom)}`)
        const dateUntil = highlight(expirationDateText)

        if (!revokeAccessEnabled || isLoginTimeValid)
        {
            //: Example: Valid from 14.05.2023 to 24.05.2023, 12:24 (by server time)
            return qsTr("Valid from %1 to %2 (by server time)")
                .arg(dateFrom)
                .arg(dateUntil)
        }

        const expires =
            highlight(`${UserSettingsGlobal.humanReadableSeconds(expiresAfterLoginS)}`)

        //: Valid from 14.05.2023 to 24.05.2023 (by server time) or for 12 hours after login
        return qsTr("Valid from %1 to %2 (by server time) or for %3 after login")
            .arg(dateFrom)
            .arg(dateUntil)
            .arg(expires)
    }

    function displayDate(date)
    {
        const time = date.getTime()
        const localDisplay = new Date(time + displayOffset(time))
        return NxGlobals.dateInShortFormat(localDisplay)
    }

    function highlight(text)
    {
        return `<b><font color="${highlightColor}">${text}</font></b>`
    }

    function displayOffset(time)
    {
        return timeOffsetProvider ? timeOffsetProvider.displayOffset(time) : 0
    }
}
