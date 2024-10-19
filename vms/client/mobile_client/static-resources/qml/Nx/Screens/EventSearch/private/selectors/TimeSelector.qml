// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core 1.0

import "../editors"

/**
 * Time selection selector. Allows to select some EventSearch::TimeSelection values.
 */
OptionSelector
{
    id: control

    text: qsTr("Period")

    unselectedValue: EventSearch.TimeSelection.anytime
    valueToTextFunc:
        (value) => EventSearchUtils.timeSelectionText(value ?? EventSearch.TimeSelection.anytime)

    screenDelegate: RadioGroupEditor
    {
        model: [EventSearch.TimeSelection.day,
           EventSearch.TimeSelection.week,
           EventSearch.TimeSelection.month]
    }
}
