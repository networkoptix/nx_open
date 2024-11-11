// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQml 2.14

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Core.EventSearch 1.0

import nx.vms.client.core.analytics 1.0 as Analytics

import "private"

Column
{
    id: analyticsFilters

    spacing: 8

    property Analytics.AnalyticsFilterModel model: null

    property var engine: null

    readonly property var selectedObjectTypeIds:
        model.getAnalyticsObjectTypeIds(impl.selectedObjectType)

    readonly property var selectedAttributeFilters: objectTypeSelector.selectedObjectType
        ? impl.selectedAttributeFilters
        : []

    // Interface functions.

    function clear()
    {
        objectTypeSelector.clear()
    }

    function setSelectedObjectTypeIds(ids)
    {
        impl.setSelectedObjectTypeIds(ids)
    }

    function setSelectedAttributeFilters(filters)
    {
        impl.setSelectedAttributeFilters(filters)
    }

    function setSelected(engine, objectTypeIds, attributeFilters)
    {
        impl.setup(engine, objectTypeIds,
            EventSearchHelpers.attributeValuesFromFilters(attributeFilters))
    }

    // Implementation.

    NxObject
    {
        id: impl

        property bool updating: false
        property var selectedAttributeFilters: []
        property var selectedObjectType: null

        // Update UI from data.

        function setup(engine, objectTypeIds, attributeValues)
        {
            const focusState = objectAttributes.getFocusState()

            updating = true

            model.setSelected(engine, attributeValues)
            analyticsFilters.engine = engine

            objectTypeSelector.objectTypes = model.objectTypes
            const filterType = findFilterObjectType(objectTypeIds)
            objectTypeSelector.setSelectedObjectType(filterType)

            objectAttributes.attributes = objectTypeSelector.selectedObjectType
                ? objectTypeSelector.selectedObjectType.attributes
                : null

            objectAttributes.selectedAttributeValues = attributeValues

            updating = false

            objectAttributes.setFocusState(focusState)

            updateAttributeFilters()
            updateSelectedObjectType()
        }

        function setSelectedObjectType(filterObjectType)
        {
            if (!filterObjectType)
            {
                objectTypeSelector.clear()
                return
            }

            if (filterObjectType.id !== objectTypeSelector.selectedObjectTypeId)
            {
                objectTypeSelector.setSelectedObjectType(filterObjectType)
            }
        }

        function findFilterObjectType(ids)
        {
            return ids && ids.length
                ? model.findFilterObjectType(ids)
                : null
        }

        function setSelectedObjectTypeIds(ids)
        {
            if (!updating)
                setSelectedObjectType(findFilterObjectType(ids))
        }

        function setSelectedAttributeFilters(filters)
        {
            if (JSON.stringify(selectedAttributeFilters) === JSON.stringify(filters))
                return

            setup(engine, selectedObjectTypeIds,
                EventSearchHelpers.attributeValuesFromFilters(filters))
        }

        Connections
        {
            target: model
            enabled: !impl.updating
            function onObjectTypesChanged()
            {
                impl.setup(
                    analyticsFilters.engine,
                    analyticsFilters.selectedObjectTypeIds,
                    /*attributeValues*/ {})
            }
        }

        // Update data from UI.

        function updateAttributeFilters()
        {
            impl.selectedAttributeFilters = EventSearchHelpers.attributeFiltersFromValues(
                objectAttributes.selectedAttributeValues)
        }

        function updateSelectedObjectType()
        {
            impl.selectedObjectType = objectTypeSelector.selectedObjectType
        }

        Connections
        {
            target: objectAttributes
            enabled: !impl.updating

            function onSelectedAttributeValuesChanged()
            {
                impl.updateAttributeFilters()
            }

            function onReferencedAttributeValueChanged()
            {
                impl.setup(
                    analyticsFilters.engine,
                    analyticsFilters.selectedObjectTypeIds,
                    objectAttributes.selectedAttributeValues)
            }
        }

        Connections
        {
            target: objectTypeSelector
            enabled: !impl.updating
            function onSelectedObjectTypeChanged()
            {
                impl.setup(
                    analyticsFilters.engine,
                    model.getAnalyticsObjectTypeIds(objectTypeSelector.selectedObjectType),
                    /*attributeValues*/ {})
            }
        }
    }

    onModelChanged:
        impl.setup(engine, selectedObjectTypeIds, /*attributeValues*/ {})

    onVisibleChanged:
    {
        if (model)
            model.active = visible
    }

    RecursiveObjectTypeSelector
    {
        id: objectTypeSelector

        objectTypes: []

        width: analyticsFilters.width
        title: qsTr("Object Type")
        visible: model.objectTypes.length > 0
    }

    ObjectAttributes
    {
        id: objectAttributes

        width: analyticsFilters.width
        loggingCategory: category
    }

    LoggingCategory
    {
        id: category
        name: "Nx.AnalyticsFilters"
    }
}
