// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core.Controls

import nx.vms.client.desktop.analytics as Analytics

/**
 * A name-value table accepting a list of `nx::vms::client::core::analytics::Attribute` as items.
 */
NameValueTable
{
    property alias /*list<DisplayedAttribute>*/ attributes: attributeFilter.attributes
    property alias /*AttributeDisplayManager*/ attributeManager: attributeFilter.manager
    property alias /*bool*/ filter: attributeFilter.filter
    property alias /*bool*/ sort: attributeFilter.sort

    items: attributeFilter.displayedAttributes
    nameRole: "displayedName"
    valuesRole: "displayedValues"
    colorsRole: "colorValues"

    Analytics.AttributeFilter
    {
        id: attributeFilter
    }
}
