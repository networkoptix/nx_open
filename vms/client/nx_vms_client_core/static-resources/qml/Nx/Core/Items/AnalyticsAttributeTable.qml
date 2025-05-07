// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core.Controls

import nx.vms.client.core.analytics

/**
 * A name-value table accepting a list of `nx::vms::client::core::analytics::Attribute` as items.
 */
NameValueTable
{
    property list<DisplayedAttribute> attributes

    items: attributes
    nameRole: "displayedName"
    valuesRole: "displayedValues"
    colorsRole: "colorValues"
}
