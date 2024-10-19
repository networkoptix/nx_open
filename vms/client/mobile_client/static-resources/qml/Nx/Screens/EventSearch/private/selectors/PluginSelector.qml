// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core 1.0

import "../editors"

/**
 * Represents event search plugin selector.
 */
OptionSelector
{
    id: control

    property var model

    text: qsTr("Plugin")
    textFieldName: "name"
    unselectedValue: null

    screenDelegate: RadioGroupEditor
    {
        model: control.model
    }
}
