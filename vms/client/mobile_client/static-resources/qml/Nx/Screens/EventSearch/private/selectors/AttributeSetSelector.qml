// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core.analytics 1.0

import "../editors"

/**
 * Represents object attribute set selector with nested values.
 */
OptionSelector
{
    id: control

    property Attribute attribute: null
    property Item subpropertyParent: null
    property string parentPropertyName: text

    property alias nestedValue: nestedAttributes.value

    valueToTextFunc: (val) =>
    {
        if (val === undefined || val === null)
            return unselectedTextValue

        return val
            ? qsTr("Present")
            : qsTr("Absent")
    }

    screenDelegate: RadioGroupEditor
    {
        model: [true, false]
    }

    ObjectAttributes
    {
        id: nestedAttributes

        parent: control.subpropertyParent
        parentPropertyName: control.parentPropertyName
        attributes: control.value && control.attribute && control.attribute.attributeSet
            ? control.attribute.attributeSet.attributes
            : null
    }
}
