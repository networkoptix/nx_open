// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

ModalDialog
{
    id: dialog

    required property Analytics.StateView taxonomy

    // Source model which is used when editing an exising list.
    property LookupListModel sourceModel
    property bool editMode: !!sourceModel

    // Model working as backend of the dialog.
    property alias viewModel: model

    property bool canAccept: model.isValid
    property int minimumLabelColumnWidth: 0

    signal deleteRequested()

    title: editMode ? qsTr("List Settings") : qsTr("New List")

    function createListTypeModel()
    {
        let result = [{ value: null, text: qsTr("Generic") }]
        return taxonomy.objectTypes.reduce(
            (list, objectType) =>
            {
                // Skip object types wihtout attributes.
                if (objectType.attributes.length == 0)
                    return list

                list.push({value: objectType, text: objectType.name})
                return list
            }, result)
    }

    function addAttributesRecursive(objectType, prefix)
    {
        if (!objectType)
        {
            console.log("WARNING: Model integrity failure")
            return;
        }

        for (let i = 0; i < objectType.attributes.length; ++i)
        {
            const attribute = objectType.attributes[i]
            const attributeName = prefix ? prefix + "." + attribute.name : attribute.name
            if (attribute.type === Analytics.Attribute.Type.attributeSet)
            {
                addAttributesRecursive(attribute.attributeSet, attributeName)
            }
            else
            {
                attributesModel.append({
                    text: attributeName,
                    checked: model.attributeNames.indexOf(attributeName) >= 0
                })
            }
        }
    }

    function populateAttributesModel()
    {
        attributesModel.clear()
        if (model.isGeneric)
            return

        addAttributesRecursive(model.listType, null)

        // Remove from the list attributes which are not available anymore.
        updateSelectedAttributes()
    }

    function updateSelectedAttributes()
    {
        let attributeNames = []
        for (let i = 0; i < attributesModel.count; ++i)
        {
            if (attributesModel.get(i).checked)
                attributeNames.push(attributesModel.get(i).text)
        }
        model.attributeNames = attributeNames
    }

    LookupListModel
    {
        id: model
        data.id: sourceModel ? sourceModel.data.id : NxGlobals.generateUuid()
        data.name: sourceModel ? sourceModel.data.name : qsTr("New List")
        data.objectTypeId: sourceModel ? sourceModel.data.objectTypeId : ""
        data.entries: sourceModel ? sourceModel.data.entries : []
        attributeNames: sourceModel ? sourceModel.attributeNames : ["Value"]

        property alias name: model.data.name
        property alias objectTypeId: model.data.objectTypeId
        property Analytics.ObjectType listType: taxonomy.objectTypeById(data.objectTypeId)
        property bool isGeneric: !listType
        property bool isValid: !!name && (isGeneric ? !!columnName : attributeNames.length)
        property string columnName: (isGeneric && attributeNames.length) ? attributeNames[0] : ""

        function setColumnName(text)
        {
            if (!isGeneric)
            {
                console.log("Cannot set column name for non-generic list")
                return
            }

            if (attributeNames)
                attributeNames[0] = text
            else
                attributeNames.push(text)
        }
    }

    ListModel
    {
        id: attributesModel
    }

    contentItem: GridLayout
    {
        columns: 2

        component AlignedLabel: Label
        {
            Layout.alignment: Qt.AlignBaseline
            Layout.minimumWidth: minimumLabelColumnWidth
            horizontalAlignment: Text.AlignRight
            Component.onCompleted:
            {
                minimumLabelColumnWidth = Math.max(minimumLabelColumnWidth, implicitWidth)
            }
        }

        AlignedLabel { text: qsTr("Name") }

        TextField
        {
            id: nameField
            Layout.fillWidth: true
            text: model.name
            onTextEdited: model.name = text
        }

        AlignedLabel { text: qsTr("Type") }

        ComboBox
        {
            id: typeField
            Layout.fillWidth: true
            textRole: "text"
            valueRole: "value"
            enabled: !editMode //< Changing list type while editing is forbidden.
            model: createListTypeModel()

            onActivated:
            {
                viewModel.objectTypeId = currentValue ? currentValue.mainTypeId : ""
                populateAttributesModel()
            }
            Component.onCompleted:
            {
                currentIndex = indexOfValue(model.listType)
                populateAttributesModel()
            }
        }

        AlignedLabel
        {
            text: qsTr("Column Name")
            visible: model.isGeneric
        }

        TextField
        {
            id: columnNameField
            Layout.fillWidth: true
            visible: model.isGeneric
            text: model.columnName
            onTextEdited: model.setColumnName(text)
        }

        AlignedLabel
        {
            text: qsTr("Attributes")
            visible: !model.isGeneric && attributesModel.count
            Layout.alignment: Qt.AlignRight | Qt.AlignTop
        }

        AnalyticsObjectAttributesSelector
        {
            Layout.fillWidth: true
            visible: !model.isGeneric && attributesModel.count
            model: attributesModel
            onSelectionChanged: updateSelectedAttributes()
        }
    }

    function accept()
    {
        if (model.isValid)
        {
            dialog.accepted()
            dialog.close()
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Cancel

        Button
        {
            text: editMode ? qsTr("OK") : qsTr("Create")
            width: Math.max(buttonBox.standardButton(DialogButtonBox.Cancel).width, implicitWidth)
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: model.isValid
            isAccentButton: true
        }
    }

    TextButton
    {
        x: 8
        anchors.verticalCenter: buttonBox.verticalCenter
        visible: editMode
        text: qsTr("Delete")
        DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        icon.source: "image://svg/skin/text_buttons/trash.svg"

        onClicked:
        {
            dialog.deleteRequested()
            dialog.reject()
        }
    }
}
