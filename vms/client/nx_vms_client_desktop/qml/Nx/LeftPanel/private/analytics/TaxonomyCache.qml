// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml 2.15

import Nx 1.0

import nx.vms.client.desktop.analytics 1.0 as Analytics

// An adapter for Analytics.TaxonomyManager.currentTaxonomy that allows to temporarily disable
// tracking of taxonomy changes (for example when an item that needs it is invisible).
NxObject
{
    id: taxonomyCache

    property Analytics.Taxonomy currentTaxonomy: null
    property bool active: true //< Whether cache update is on.

    Binding
    {
        target: taxonomyCache
        when: taxonomyCache.active
        property: "currentTaxonomy"
        value: Analytics.TaxonomyManager.currentTaxonomy
        restoreMode: Binding.RestoreNone
    }
}
