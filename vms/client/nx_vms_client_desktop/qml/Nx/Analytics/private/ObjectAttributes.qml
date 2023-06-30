// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0

import nx.vms.client.core.analytics 1.0 as Analytics

Column
{
    id: objectAttributes

    property var attributes
    property string prefix

    property var loggingCategory

    property var selectedAttributeValues: {}

    /* Emitted with delay when a referenced attribute value is changed. */
    signal referencedAttributeValueChanged()

    spacing: 8

    onAttributesChanged:
    {
        impl.setAttributes(attributes)
    }

    function getFocusState()
    {
        let activeEditor = impl.editorList.find(editor => editor.activeFocus)

        if (!activeEditor)
            return null

        return {
            name: activeEditor.attribute.name,
            editorState: activeEditor.getFocusState ? activeEditor.getFocusState() : null
        }
    }

    function setFocusState(state)
    {
        let editor = state
            ? impl.editorList.find(editor => editor.attribute.name === state.name)
            : null

        if (!editor)
            return

        if (editor.setFocusState)
            editor.setFocusState(state.editorState)

        editor.forceActiveFocus()
    }

    Repeater
    {
        id: editors

        AttributeEditor
        {
            id: attributeEditor

            attribute: modelData
            width: objectAttributes.width
            prefix: objectAttributes.prefix
            loggingCategory: objectAttributes.loggingCategory

            contentEnabled: attributeEditor.selectedValue !== undefined
                || objectAttributes.selectedAttributeValues[modelData.name] === undefined

            onSelectedValueChanged: impl.editorValuesChanged(attributeEditor)
            onNestedSelectedValuesChanged: impl.editorValuesChanged(attributeEditor)
            onActiveFocusChanged:
            {
                if (!activeFocus)
                    impl.editorValuesChanged(attributeEditor)
            }
        }
    }

    NxObject
    {
        id: impl

        property bool updating: false
        property var editorList: []

        Connections
        {
            target: objectAttributes
            enabled: !impl.updating
            function onSelectedAttributeValuesChanged()
            {
                impl.setSelectedAttributeValues(objectAttributes.selectedAttributeValues)
            }
        }

        function setAttributes(attributes)
        {
            updating = true

            editors.model = orderedAttributes(attributes)
            editorList = Array.from(objectAttributes.children).filter(
                item => item instanceof AttributeEditor)

            updating = false

            updateSelectedAttributeValues()
        }

        function orderedAttributes(attributes)
        {
            if (!attributes)
                return null

            const attributeByName = {}
            const orderedAttributeNames = []
            for (const attribute of attributes)
            {
                attributeByName[attribute.name] = attribute
                orderedAttributeNames.push(attribute.name)
            }

            return orderedAttributeNames.map(attributeName => attributeByName[attributeName])
        }

        Timer
        {
            id: delayedReferencedAttributeSignal
            interval: 2000
            onTriggered:
            {
                objectAttributes.referencedAttributeValueChanged()
            }
        }

        function editorValuesChanged(editor)
        {
            if (updating)
                return

            updateSelectedAttributeValues()
            if (editor.attribute.isReferencedInCondition)
            {
                if (editor.hasTextFields && editor.activeFocus)
                {
                    delayedReferencedAttributeSignal.restart();
                }
                else
                {
                    delayedReferencedAttributeSignal.stop();
                    objectAttributes.referencedAttributeValueChanged()
                }
            }
        }

        function setSelectedAttributeValues(nameValueTree)
        {
            updating = true

            delayedReferencedAttributeSignal.stop()

            for (let child of editorList)
            {
                console.assert(child.attribute)
                if (child.attribute)
                    child.setSelectedValue(nameValueTree[child.attribute.name])
            }

            updating = false

            updateSelectedAttributeValues()
        }

        function updateSelectedAttributeValues()
        {
            updating = true
            objectAttributes.selectedAttributeValues = currentAttributeValues()
            updating = false
        }

        function currentAttributeValues()
        {
            return editorList.reduce(
                function(result, item)
                {
                    if (item.selectedValue === undefined)
                        return result

                    if (item.nestedSelectedValues !== undefined)
                    {
                        if (Object.keys(item.nestedSelectedValues).length > 0)
                        {
                            for (const name in item.nestedSelectedValues)
                            {
                                result[`${item.attribute.name}.${name}`] =
                                    item.nestedSelectedValues[name]
                            }
                        }
                        else
                        {
                            result[item.attribute.name] = item.selectedValue
                                ? (name => `${name}=true`)
                                : (name => `${name}!=true`)
                        }
                    }
                    else
                    {
                        result[item.attribute.name] = item.selectedValue
                    }

                    return result
                },
                {})
        }
    }
}
