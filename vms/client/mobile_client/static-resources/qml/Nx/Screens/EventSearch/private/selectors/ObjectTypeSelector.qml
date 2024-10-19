// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core.Controls
import Nx.Mobile.Controls

import nx.vms.client.core.analytics

import "."
import "../editors"

/**
 * Represents single object type entity selector.
 */
OptionSelector
{
    id: control

    property var objectTypes

    textFieldName: "name"
    compareFunc: (left, right) =>
    {
        const getId = (val) => (val && val.id) ?? undefined
        return getId(left) === getId(right)
    }

    onObjectTypesChanged:
    {
        if (!objectTypes)
            return

        const getId = (value) => value ? value.id : undefined
        const contains = !!objectTypes.find((value) => getId(control.value) === getId(value))
        if (!contains && value !== null)
            value = null
    }

    visualDelegate: ColoredImage
    {
        id: visualDelegateItem

        property var data
        property Text textValueItem

        width: !!data
            ? 32
            : 20
        height: width
        sourceSize: Qt.size(width, height)
        visible: sourcePath !== ""
        primaryColor: textValueItem
            ? textValueItem.color
            : "transparent"
        sourcePath:
        {
            if (!textValueItem)
                return ""

            return !!data
                ? IconManager.iconUrl(data["iconSource"])
                : (control.value && IconManager.iconUrl(control.value["iconSource"])) ?? ""
        }
    }

    screenDelegate: GridEditor
    {
        model: control.objectTypes
        visualDelegate: control.visualDelegate
    }
}
