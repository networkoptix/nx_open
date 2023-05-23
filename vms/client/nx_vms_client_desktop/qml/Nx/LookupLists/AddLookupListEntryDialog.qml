// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

ModalDialog
{
    id: dialog

    required property Analytics.StateView taxonomy
    required property LookupListModel model
    property bool isGeneric: !taxonomy.objectTypeById(model.data.objectTypeId)
    property var entry: ({})

    function updateAddButton()
    {
        addButton.enabled = Object.keys(entry).length > 0
    }

    title: qsTr("Add Entry")

    contentItem: Column
    {
        topPadding: padding / 2
        spacing: 6

        Repeater
        {
            id: repeater

            model: dialog.model.attributeNames

            GridLayout
            {
                columnSpacing: 6
                width: parent.width
                columns: isGeneric ? 2 : 1

                Label
                {
                    text: modelData
                    Layout.alignment: Qt.AlignBaseline
                    horizontalAlignment: isGeneric ? Text.AlignRight : Text.AlignLeft
                }
                LookupListElementDelegate
                {
                    taxonomy: dialog.taxonomy
                    objectTypeId: dialog.model.data.objectTypeId
                    attributeName: modelData

                    Layout.fillWidth: true
                    onValueChanged: (value) =>
                    {
                        // Value is a string or undefined, empty string is considered invalid.
                        // Also there is a separate check for zero numeric values.
                        if (value || value === 0)
                            entry[modelData] = value
                        else
                            delete entry[modelData]
                        updateAddButton()
                    }
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Cancel

        Button
        {
            id: addButton

            text: qsTr("Add")
            width: Math.max(buttonBox.standardButton(DialogButtonBox.Cancel).width, implicitWidth)
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: false
            isAccentButton: true
        }
    }
}
