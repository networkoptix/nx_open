// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

Control
{
    id: control

    required property Analytics.ObjectType objectType
    required property Analytics.Attribute attribute

    property bool isGeneric: !objectType
    property string value

    signal editingStarted
    signal editingFinished

    function setFocus()
    {
        control.editingStarted()
        loader.item.forceActiveFocus()
    }

    component ComboBoxBasedEditor: ComboBox
    {
        Connections
        {
            target: control
            function onValueChanged() { currentIndex = indexOfValue(control.value) }
        }

        onActivated: (index) =>
        {
            control.editingStarted()
            control.value = index ? valueAt(index) : qsTr("Any %1").arg(attribute.name)
            control.editingFinished()
        }

        Component.onCompleted: currentIndex = indexOfValue(control.value)

        onActiveFocusChanged:
        {
            if (!activeFocus)
                control.editingFinished()
        }
    }

    Component
    {
        id: textDelegate

        TextField
        {
            Connections
            {
                target: control
                function onValueChanged() { text = control.value }
            }

            onTextChanged: control.value = text
            onEditingFinished:
            {
                if (!contextMenuOpening)
                    control.editingFinished()
            }

            Component.onCompleted: text = control.value
        }
    }

    Component
    {
        id: numberDelegate

        NumberInput
        {
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

            onValueChanged: control.value = value || ""
            onEditingFinished:
            {
                if (!contextMenuOpening)
                    control.editingFinished()
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
        }
        return textDelegate
    }

    contentItem: Loader
    {
        id: loader
        sourceComponent: calculateComponent()
    }
}
