// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core.analytics

import "../editors"

/**
 * Object attribute selector for enumeration values.
 */
OptionSelector
{
    id: control

    property Attribute attribute: null

    screenDelegate: RadioGroupEditor
    {
        model: CoreUtils.toArray(attribute && attribute.enumeration && attribute.enumeration.items)
    }
}
