// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import nx.vms.client.desktop.analytics 1.0 as Analytics

import ".."

GenericTypeSelector
{
    id: control

    property var objectTypes: []
    property string selectedObjectTypeId: selectedTypeId ?? ""
    readonly property var selectedObjectType: objectTypeById[selectedObjectTypeId] || null

    readonly property var objectTypeById: Array.prototype.reduce.call(objectTypes,
        function(result, objectType)
        {
            result[objectType.id] = objectType
            return result
        },
        {})

    value: selectedObjectType ? selectedObjectType.name : ""

    model: objectTypes

    iconRole: "iconSource"
    iconPath: (iconName => Analytics.IconManager.iconUrl(iconName))
}
