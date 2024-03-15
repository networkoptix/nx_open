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

import "taxonomy_utils.js" as TaxonomyUtils

ModalDialog
{
    id: dialog

    required property Analytics.StateView taxonomy
    required property LookupListModel model

    property Analytics.ObjectType objectType: taxonomy.objectTypeById(model.data.objectTypeId)
    property var entry: ({})

    function updateAddButton()
    {
        addButton.enabled = Object.keys(entry).length > 0
    }

    minimumWidth: 450
    minimumHeight: 213
    title: qsTr("Add Entry")

    contentItem: Scrollable
    {
        id: table

        height: 132
        scrollView.ScrollBar.vertical.x: table.width - scrollView.ScrollBar.vertical.width + padding

        contentItem: Column
        {
            id: entries

            width: parent.width
            topPadding: padding / 2
            spacing: 8

            Repeater
            {
                model: dialog.model.attributeNames

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: 6

                    Label
                    {
                        Layout.preferredWidth: 70
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                        text: modelData
                    }

                    LookupListElementEditor
                    {
                        Layout.preferredWidth: 340
                        objectType: dialog.objectType
                        attribute: TaxonomyUtils.findAttribute(objectType, modelData)

                        onValueChanged:
                        {
                            if (isDefinedValue())
                                entry[modelData] = value
                            else
                                delete entry[modelData]
                            updateAddButton()
                        }
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
