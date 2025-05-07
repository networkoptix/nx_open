// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core.Items

import nx.vms.client.desktop.analytics as Analytics

/**
 * AnalyticsAttributeTable with the ability to sort and filter displayed attributes
 * using specified AttributeDisplayManager.
 */
AnalyticsAttributeTable
{
    property alias /*list<DisplayedAttribute>*/ attributes: attributeFilter.attributes
    property alias /*AttributeDisplayManager*/ attributeManager: attributeFilter.manager
    property alias /*bool*/ filter: attributeFilter.filter
    property alias /*bool*/ sort: attributeFilter.sort

    items: attributeFilter.displayedAttributes

    Analytics.AttributeFilter
    {
        id: attributeFilter
    }
}
