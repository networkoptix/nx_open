// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQml 2.14

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop.analytics 1.0 as Analytics

import "private"

Column
{
    id: analyticsFilters

    spacing: 8

    property Analytics.AnalyticsFilterModel model: null

    readonly property var selectedAnalyticsObjectTypeIds:
        model.getAnalyticsObjectTypeIds(objectTypeSelector.selectedObjectType)

    readonly property var selectedAttributeValues:
    {
        if (!objectTypeSelector.selectedObjectType)
            return []

        function quoteIfNeeded(text, quoteNumbers)
        {
            if (text.search(/\s/) >= 0)
                return `"${text}"`

            return (quoteNumbers && text.match(/^[+-]?\d+(\.\d+)?$/))
                ? `"${text}"`
                : text
        }

        var result = []
        const values = objectAttributes.selectedAttributeValues

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

    function clear()
    {
        objectTypeSelector.clear()
    }

    function setSelectedAnalyticsObjectTypeIds(ids)
    {
        let filterObjectType = ids && ids.length
            ? model.findFilterObjectType(ids)
            : null

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

    function setSelectedAttributeValues(stringList)
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

        objectAttributes.setSelectedAttributeValues(attributeTree)
    }

    onVisibleChanged:
    {
        if (model)
            model.active = visible
    }

    RecursiveObjectTypeSelector
    {
        id: objectTypeSelector

        width: analyticsFilters.width
        title: qsTr("Object Type")
        objectTypes: model.objectTypes
        visible: model.objectTypes.length > 0
    }

    ObjectAttributes
    {
        id: objectAttributes

        width: analyticsFilters.width
        attributes: objectTypeSelector.selectedObjectType
            ? objectTypeSelector.selectedObjectType.attributes
            : null
        loggingCategory: category
    }

    LoggingCategory
    {
        id: category
        name: "Nx.LeftPanel.AnalyticsFilters"
    }
}
