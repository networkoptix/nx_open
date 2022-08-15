// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import nx.vms.client.desktop.analytics 1.0 as Analytics

Column
{
    id: control

    property alias objectTypes: selector.objectTypes

    property alias title: selector.name

    property Analytics.Engine engine: null

    readonly property Analytics.ObjectType selectedObjectType:
        derivedSelectorLoader.item && derivedSelectorLoader.item.selectedObjectType
            ? derivedSelectorLoader.item.selectedObjectType
            : selector.selectedObjectType

    readonly property string selectedObjectTypeId: selectedObjectType
        ? selectedObjectType.id
        : null

    function allowedObjectTypes(objectTypes)
    {
        return Array.prototype.filter.call(objectTypes,
            function (objectType)
            {
                return objectType.isReachable && !objectType.isLiveOnly && !objectType.isNonIndexable
                    && Analytics.TaxonomyManager.isRelevantForEngine(objectType, engine)
            })
    }

    function clear()
    {
        selector.clear()
    }

    function setSelectedObjectType(value)
    {
        if (value && value === selectedObjectType)
            return

        let hierarchy = []
        for (let type = value; !!type; type = type.baseType)
            hierarchy.unshift(type)

        selectRecursively(hierarchy)
    }

    spacing: 8

    ObjectTypeSelector
    {
        id: selector

        width: control.width
        name: qsTr("Object Type")

        readonly property var derivedTypes: selectedObjectType
            ? allowedObjectTypes(selectedObjectType.derivedTypes)
            : []
    }

    Loader
    {
        id: derivedSelectorLoader

        width: control.width

        source: selector.derivedTypes.length > 0
            ? "RecursiveObjectTypeSelector.qml"
            : ""

        visible: status === Loader.Ready

        Binding
        {
            target: derivedSelectorLoader.item
            property: "objectTypes"
            value: selector.derivedTypes
        }

        Binding
        {
            target: derivedSelectorLoader.item
            property: "engine"
            value: control.engine
        }

        Binding
        {
            target: derivedSelectorLoader.item
            property: "title"

            value:
            {
                if (!selector.selectedObjectType)
                    return ""

                const subtype = qsTr("Subtype")
                return `${selector.selectedObjectType.name} ${subtype}`
            }
        }
    }

    function selectRecursively(hierarchy) //< Internal.
    {
        clear()

        if (!hierarchy.length)
            return

        const type = hierarchy.shift()
        selector.selectedTypeId = type.id

        if (derivedSelectorLoader.item)
            derivedSelectorLoader.item.selectRecursively(hierarchy)
        else
            console.assert(!hierarchy.length)
    }
}
