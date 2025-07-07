// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core.Controls

import nx.vms.client.core.analytics

/**
 * A name-value table accepting a list of `nx::vms::client::core::analytics::Attribute` as items.
 */
NameValueTable
{
    // For some reason with a typed list property a `Binding` to it causes a crash in Qt 6.9.1,
    // so it's just `var` as a workaround.
    property /*list<DisplayedAttribute>*/ var attributes

    items: attributes
    nameRole: "displayedName"
    valuesRole: "displayedValues"
    colorsRole: "colorValues"
}
