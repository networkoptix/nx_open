// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

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
        editor.value = model.display
        isEditing = true
        editor.setFocus()
    }

    function commit()
    {
        const valueChanged = editor.value !== model.display
        const previousValue = model.display
        model.edit = editor.value
        isEditing = false
        if (valueChanged)
            control.valueChanged(model.edit, previousValue)
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
        onClicked: control.edit()
        visible: !control.isEditing
    }

    LookupListElementViewer
    {
        value: model.display ?? ""
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
        value: model.display

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
                    control.revert(model.display)
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
