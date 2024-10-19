// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Core.EventSearch 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.core.analytics 1.0

import "."

/**
 * Set of standard analytics search selectors.
 */
Column
{
    id: control

    property CommonObjectSearchSetup searchSetup: null
    property AnalyticsSearchSetup analyticsSearchSetup: null

    spacing: 4
    width: (parent && parent.width) ?? 0

    PluginSelector
    {
        id: pluginSelector

        model: d.model.engines
        visible: d.model.engines.length > 1
    }

    RecursiveObjectTypeSelector
    {
        id: objectTypeSelector

        objectTypes: d.model.objectTypes ?? null
        visible: d.model.objectTypes && d.model.objectTypes.length
    }

    Text
    {
        text: qsTr("ADVANCED SEARCH")
        font.pixelSize: 14
        color: ColorTheme.colors.light12
        visible: objectAttributes.hasAttributes
        topPadding: 16
        bottomPadding: 16
        leftPadding: 16
    }

    ObjectAttributes
    {
        id: objectAttributes

        attributes: objectTypeSelector.value && objectTypeSelector.value.attributes
    }

    NxObject
    {
        id: d

        readonly property AnalyticsFilterModel model:
        {
            const result = systemContext.taxonomyManager.createFilterModel()
            result.selectedDevices = Qt.binding(() => searchSetup.selectedCameras)
            result.selectedEngine = Qt.binding(() => getCurrentEngine(result))
            result.selectedAttributeValues = Qt.binding(() => objectAttributes.value ?? {})
            return result
        }

        function getCurrentEngine(model)
        {
            const engineId = control.analyticsSearchSetup.engine.toString()
            return model.engines.find(engine => engine.id === engineId) ?? null
        }

        Component.onCompleted:
        {
            // Plugin selector setup.
            pluginSelector.value = d.getCurrentEngine(d.model)
            control.analyticsSearchSetup.engine = Qt.binding(
                () => NxGlobals.uuid((pluginSelector.value && pluginSelector.value.id) ?? ""))

            // Object type selector setup.
            objectTypeSelector.value = Qt.binding(
                () => d.model.findFilterObjectType(control.analyticsSearchSetup.objectTypes))

            control.analyticsSearchSetup.objectTypes = Qt.binding(
                () => d.model.getAnalyticsObjectTypeIds(objectTypeSelector.value))

            // Object attributes selector setup.
            objectAttributes.value = Qt.binding(() =>
                EventSearchHelpers.attributeValuesFromFilters(
                    control.analyticsSearchSetup.attributeFilters))

            control.analyticsSearchSetup.attributeFilters = Qt.binding(
                () => EventSearchHelpers.attributeFiltersFromValues(objectAttributes.value))
        }
    }
}
