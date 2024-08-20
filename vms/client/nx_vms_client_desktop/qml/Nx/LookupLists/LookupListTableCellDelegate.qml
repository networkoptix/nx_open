// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.core.analytics as Analytics

import "taxonomy_utils.js" as TaxonomyUtils

FocusScope
{
    id: control

    required property Analytics.StateView taxonomy
    property Analytics.ObjectType objectType: taxonomy.objectTypeById(model.objectTypeId)
    property Analytics.Attribute attribute:
        TaxonomyUtils.findAttribute(objectType, model.attributeName)

    property bool isEditing: false
    focus: isEditing
    anchors.fill: parent

    signal editingStarted
    signal editingFinished
    signal valueChanged(var newValue, var previousValue)

    // Never pass key presses to parents while editing.
    Keys.onPressed: (event) => event.accepted = isEditing

    function edit()
    {
        control.editingStarted()
        editor.value = model.rawValue
        isEditing = true
        editor.startEditing()
    }

    function commit()
    {
        if (!isEditing)
            return

        const previousValue = model.rawValue
        const newValue = editor.value

        isEditing = false

        if (previousValue === newValue)
            return

        model.edit = newValue
        control.valueChanged(newValue, previousValue)
    }

    function revert(valueRevertTo)
    {
        editor.value = valueRevertTo
        model.edit = editor.value
        isEditing = false
        control.editingFinished()
    }

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: control.edit()
        visible: !control.isEditing
    }

    LookupListElementViewer
    {
        displayValue: model.display ?? ""
        value: model.rawValue ?? ""
        colorHexValue: model.colorRGBHex ?? ""
        visible: !control.isEditing
        anchors.fill: parent

        objectType: control.objectType
        attribute: control.attribute
    }

    LookupListElementEditor
    {
        id: editor

        visible: control.isEditing
        anchors.fill: parent
        focus: true
        clip: true
        value: model.rawValue

        objectType: control.objectType
        attribute: control.attribute

        onEditingStarted: control.editingStarted()

        onEditingFinished:
        {
            commit()
            control.editingFinished()
        }

        Keys.onPressed: (event) =>
        {
            event.accepted = true
            switch (event.key)
            {
                case Qt.Key_Enter:
                case Qt.Key_Return:
                    control.commit()
                    break

                case Qt.Key_Escape:
                    control.revert(model.rawValue)
                    break

                default:
                    event.accepted = false
                    break
            }
        }

        Keys.onShortcutOverride: (event) =>
        {
            switch (event.key)
            {
                case Qt.Key_Enter:
                case Qt.Key_Return:
                case Qt.Key_Escape:
                    event.accepted = true
                    break
            }
        }
    }
}
