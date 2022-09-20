// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import nx.vms.client.desktop.analytics 1.0 as Analytics

import ".."

GenericTypeSelector
{
    id: control

    property alias objectTypes: control.model
    readonly property var selectedObjectType: objectTypeById[selectedTypeId] || null
    readonly property var objectTypeById:
        objectTypes.reduce((result, objectType) =>
        {
            result[objectType.id] = objectType
            return result
        }, {})

    iconRole: "iconSource"
    iconPath: (iconName => Analytics.IconManager.iconUrl(iconName))
}
