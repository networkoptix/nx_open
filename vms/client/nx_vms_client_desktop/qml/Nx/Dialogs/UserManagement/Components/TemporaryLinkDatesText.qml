// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core

import nx.vms.client.desktop

Text
{
    id: temporaryUserText

    property date linkValidFrom
    property date linkValidUntil
    property int displayOffsetMs: 0
    property int expiresAfterLoginS: -1
    property bool revokeAccessEnabled: false

    text:
    {
        const dateFrom = highlight(`${displayDate(linkValidFrom)}`)
        const dateUntil = highlight(`${displayDate(linkValidUntil)}`)

        if (!revokeAccessEnabled)
            return qsTr(`Valid from %1 to %2`).arg(dateFrom).arg(dateUntil)

        const expires = highlight(`${UserSettingsGlobal.humanReadableSeconds(expiresAfterLoginS)}`)

        return qsTr(`Valid from %1 to %2 or %3 after login`)
            .arg(dateFrom)
            .arg(dateUntil)
            .arg(expires)
    }

    function displayDate(date)
    {
        const localDisplay = new Date(date.getTime() + displayOffsetMs)
        return NxGlobals.dateInShortFormat(localDisplay)
    }

    function highlight(text)
    {
        return `<b><font color="${ColorTheme.colors.light10}">${text}</font></b>`
    }

    wrapMode: Text.WordWrap
    textFormat: Text.StyledText

    color: ColorTheme.colors.light16
    font: Qt.font({pixelSize: 14, weight: Font.Normal})
    width: parent.width
}
