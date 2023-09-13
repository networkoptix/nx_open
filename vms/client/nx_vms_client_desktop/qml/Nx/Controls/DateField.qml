// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Controls

import nx.vms.client.desktop

TextField
{
    id: control

    property date date: new Date()
    property var minimum
    property var maximum

    readonly property string dateText: NxGlobals.dateInShortFormat(date)
    readonly property string kSectionSeparator: validator.dateSeparator
    property int currentSection: getCurrentSection()

    implicitWidth: 120

    validator: DateValidator
    {
        id: validator
        minimum: control.minimum
        maximum: control.maximum
    }

    Binding on text { value: dateText }

    signal dateEdited(date date)

    onFocusChanged:
    {
        if (focus && focusReason === Qt.TabFocusReason)
        {
            cursorPosition = 0
            selectCurrentSection()
        }

        if (!focus && !acceptableInput)
            text = dateText
    }

    onTextEdited:
    {
        if (acceptableInput)
            updateDate()
    }

    onCursorPositionChanged:
    {
        currentSection = getCurrentSection()
    }

    onCurrentSectionChanged:
    {
        if (!acceptableInput)
        {
            const currentCursorPosition = cursorPosition
            text = dateText
            cursorPosition = currentCursorPosition
        }
    }

    function getCurrentSection()
    {
        let result = 0
        for (let i = 0; i < cursorPosition; ++i)
        {
            if (text[i] === kSectionSeparator)
                ++result
        }
        return result
    }

    function updateDate()
    {
        date = validator.dateFromString(text)
        dateEdited(date)
    }

    function getNextSection(position)
    {
        const result = text.indexOf(kSectionSeparator, position ?? cursorPosition)
        return result !== -1 ? result + 1 : null
    }

    function nextSection()
    {
        const nextSection = getNextSection()
        if (nextSection !== null)
        {
            cursorPosition = nextSection
            selectCurrentSection()
        }

        return nextSection !== null
    }

    function selectCurrentSection()
    {
        const sectionPosition =
            text.slice(0, cursorPosition).lastIndexOf(kSectionSeparator) + 1

        const nextSection = getNextSection()
        select(sectionPosition, (nextSection ? nextSection - 1 : text.length))
    }

    Keys.onTabPressed: (event) =>
    {
        if (!nextSection())
            event.accepted = false
    }
}
