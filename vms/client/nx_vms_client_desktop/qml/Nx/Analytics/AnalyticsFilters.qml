// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQml 2.14

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop.analytics 1.0 as Analytics

import "private"

Column
{
    id: analyticsFilters

    spacing: 8

    property Analytics.AnalyticsFilterModel model: null

    property var engine: null

    readonly property var selectedAnalyticsObjectTypeIds:
        model.getAnalyticsObjectTypeIds(impl.selectedObjectType)

    readonly property var selectedAttributeFilters: objectTypeSelector.selectedObjectType
        ? impl.selectedAttributeFilters
        : []

    onModelChanged: impl.setup(engine, selectedAnalyticsObjectTypeIds, /*attributeValues*/ {})

    NxObject
    {
        id: impl

        property bool updating: false
        property var selectedAttributeFilters: []
        property var selectedObjectType: null

        function setup(engine, analyticsObjectTypeIds, attributeValues)
        {
            let focusState = objectAttributes.getFocusState()

            updating = true

            model.setSelected(engine, attributeValues)
            analyticsFilters.engine = engine

            objectTypeSelector.objectTypes = model.objectTypes
            let filterType = findFilterObjectType(analyticsObjectTypeIds)
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

        function updateAttributeFilters()
        {
            impl.selectedAttributeFilters =
                attributeFiltersFromValues(objectAttributes.selectedAttributeValues)
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
                    analyticsFilters.selectedAnalyticsObjectTypeIds,
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

        Connections
        {
            target: model
            enabled: !impl.updating
            function onObjectTypesChanged()
            {
                impl.setup(
                    analyticsFilters.engine,
                    analyticsFilters.selectedAnalyticsObjectTypeIds,
                    /*attributeValues*/ {})
            }
        }
    }

    function clear()
    {
        objectTypeSelector.clear()
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

    function setSelectedAnalyticsObjectTypeIds(ids)
    {
        setSelectedObjectType(findFilterObjectType(ids))
    }

    function setSelectedAttributeFilters(filters)
    {
        if (JSON.stringify(selectedAttributeFilters) !== JSON.stringify(filters))
            impl.setup(engine, selectedAnalyticsObjectTypeIds, attributeValuesFromFilters(filters))
    }

    function setSelected(engine, analyticsObjectTypeIds, attributeFilters)
    {
        impl.setup(engine, analyticsObjectTypeIds, attributeValuesFromFilters(attributeFilters))
    }

    function attributeValuesFromFilters(stringList)
    {
        // This simple regular expression is intended to parse back a generated attribute string.
        // To parse any valid user input it should be further improved.
        //
        // Currently it matches:
        //    any number of whitespaces
        //         -FOLLOWED BY-
        //
        //    any one or more characters between quotation marks, except !, =, : or "
        //                            -OR-
        //    any one or more characters, except !, =, :, " or a whitespace
        //
        //         -FOLLOWED BY-
        //    zero or more whitespaces
        //         -FOLLOWED BY-
        //    optional !
        //         -FOLLOWED BY-
        //    = or :
        //         -FOLLOWED BY-
        //    zero or more whitespaces
        //         -FOLLOWED BY-
        //
        //    any one or more characters between quotation marks, except "
        //                            -OR-
        //    any one or more non-whitespace characters
        //
        //         -FOLLOWED BY-
        //    any number of whitespaces

        const regex = /\s*("[^!=:"]+"|[^"!=:\s]+)\s*(!*=|:)\s*("[^"]+"|\S+)\s*$/
        let attributeTree = {}

        for (let i = 0; i < stringList.length; ++i)
        {
            const match = regex.exec(stringList[i])
            if (!match)
            {
                console.warn(loggingCategory, `Malformed filter: ${stringList[i]}`)
                continue
            }

            function unquote(str)
            {
                return str[0] == "\""
                    ? str.substr(1, str.length - 2)
                    : str
            }

            const fullName = unquote(match[1])
            const path = fullName.split(".")
            let value = unquote(match[3])
            const operation = match[2]

            if (operation === "!=")
            {
                // Currently supported only "!= true" for object values.
                const booleanTrue = /true/i.test(value)
                if (!booleanTrue)
                {
                    console.warn(loggingCategory, `Unsupported filter: ${match[0]}`)
                    continue
                }

                // Internally we replace "!= true" logic to "== false".
                value = "false"
            }

            let obj = attributeTree
            while (path.length > 1)
            {
                const name = path.shift()
                if (typeof obj[name] !== "object")
                    obj[name] = {}

                obj = obj[name]
            }

            obj[path[0]] = value
        }

        return attributeTree
    }

    function attributeFiltersFromValues(values)
    {
        function quoteIfNeeded(text, quoteNumbers)
        {
            if (text.search(/\s/) >= 0)
                return `"${text}"`

            return (quoteNumbers && text.match(/^[+-]?\d+(\.\d+)?$/))
                ? `"${text}"`
                : text
        }

        let result = []
        for (let name in values)
        {
            const value = values[name]
            if (typeof value === "function")
                result.push(value(quoteIfNeeded(name)))
            else
                result.push(`${quoteIfNeeded(name)}=${quoteIfNeeded(value.toString(), true)}`)
        }

        return result
    }

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
