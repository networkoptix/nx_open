// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.core.analytics as Analytics

Control
{
    id: control

    required property Analytics.ObjectType objectType
    required property Analytics.Attribute attribute

    property bool isGeneric: !objectType
    // Displayed value. For complex types (enum, color, bool) could differ from entry value.
    // For number and text the value is the same as entry value.
    // If you want to check whether current control value is not defined ("Any ..."),
    // use the corresponding `isDefinedValue()` function
    property string value
    property bool editing: false

    signal editingStarted
    signal editingFinished

    function isDefinedValue()
    {
        return loader.item.isDefinedValue()
    }

    function startEditing()
    {
        editing = true
        loader.item.forceActiveFocus()
        editingStarted()
    }

    function finishEditing()
    {
        editing = false
        editingFinished()
    }

    component ComboBoxBasedEditor: ComboBox
    {
        function isIndexRelatedToValue(index)
        {
            return index > 0
        }

        function isDefinedValue()
        {
            return isIndexRelatedToValue(currentIndex)
        }

        function initCurrentIndex()
        {
            const index = find(control.value)
            if (index !== -1)
                currentIndex = index
        }

        Connections
        {
            target: control
            function onValueChanged() { initCurrentIndex() }
        }

        onActivated: (index) =>
        {
            control.value = isIndexRelatedToValue(index) ? valueAt(index) : ""
            control.finishEditing()
        }

        Component.onCompleted: initCurrentIndex()

        onActiveFocusChanged:
        {
            if (!activeFocus && control.editing)
                control.finishEditing()
        }
    }

    Component
    {
        id: textDelegate

        TextField
        {
            function isDefinedValue()
            {
                // Considered defined if value is not null and not empty string.
                return control.value
            }

            placeholderText: control.value ? control.value : qsTr("Any %1").arg(attribute ? attribute.name : "value")

            Connections
            {
                target: control
                function onValueChanged() { text = control.value }
            }

            onTextChanged: control.value = text
            onEditingFinished:
            {
                if (!contextMenuOpening)
                    control.finishEditing()
            }

            Component.onCompleted: text = control.value
        }
    }

    Component
    {
        id: numberDelegate

        NumberInput
        {
            function isDefinedValue()
            {
                // Considered defined if value is not null and not empty string.
                return control.value
            }

            placeholderText: control.value ? control.value : qsTr("Any %1").arg(attribute.name)
            minimum: attribute.minValue
            maximum: attribute.maxValue

            mode: attribute.subtype === "int"
                ? NumberInput.Integer
                : NumberInput.Real

            Connections
            {
                target: control
                function onValueChanged() { setValue(control.value) }
            }

            onValueChanged:
            {
                // Have to check explicitly on undefined, since 0 is a valid value.
                control.value = value === undefined ? "" : value
            }

            onEditingFinished:
            {
                if (!contextMenuOpening)
                    control.finishEditing()
            }
            Component.onCompleted: setValue(control.value)
        }
    }

    Component
    {
        id: booleanDelegate

        ComboBoxBasedEditor
        {
            textRole: "text"
            valueRole: "value"
            model: [
                {"text": qsTr("Any %1").arg(attribute.name), "value": ""},
                {"text": qsTr("Yes"), "value": "true"},
                {"text": qsTr("No"), "value": "false"}
            ]
        }
    }

    Component
    {
        id: colorDelegate

        ComboBoxBasedEditor
        {
            withColorSection: true
            textRole: "name"
            valueRole: "color"
            model:
            {
                return [{"name": qsTr("Any %1").arg(attribute.name), "value": ""}].concat(
                    Array.prototype.map.call(attribute.colorSet.items,
                        name => ({"name": name, "color": attribute.colorSet.color(name)})))
            }
        }
    }

    Component
    {
        id: enumDelegate

        ComboBoxBasedEditor
        {
            id: enumCombobox

            textRole: "text"
            valueRole: "value"
            model:
            {
                return [{"text": qsTr("Any %1").arg(attribute.name), "value": ""}].concat(
                    Array.prototype.map.call(attribute.enumeration.items,
                        text => ({"text": text, "value": text})))
            }
        }
    }

    Component
    {
        id: objectDelegate

        ComboBoxBasedEditor
        {
            textRole: "text"
            valueRole: "value"
            model: [
                {"text": qsTr("Any %1").arg(attribute.name), "value": ""},
                {"text": qsTr("Present"), "value": "true"},
                {"text": qsTr("Absent"), "value": "false"}
            ]
        }
    }

    function calculateComponent()
    {
        if (isGeneric || !attribute)
            return textDelegate

        switch (attribute.type)
        {
            case Analytics.Attribute.Type.number:
                return numberDelegate

            case Analytics.Attribute.Type.boolean:
                return booleanDelegate

            case Analytics.Attribute.Type.string:
                return textDelegate

            case Analytics.Attribute.Type.colorSet:
                return colorDelegate

            case Analytics.Attribute.Type.enumeration:
                return enumDelegate

            case Analytics.Attribute.Type.attributeSet:
                return objectDelegate
        }
        return textDelegate
    }

    contentItem: Loader
    {
        id: loader
        sourceComponent: calculateComponent()
    }
}
