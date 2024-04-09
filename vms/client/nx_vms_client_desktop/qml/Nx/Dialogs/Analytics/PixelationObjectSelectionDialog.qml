// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.core.analytics as Analytics
import nx.vms.client.desktop.analytics as Analytics

Dialog
{
    id: dialog

    property alias allObjects: allObjects.checked
    property var objectTypeIds: []

    readonly property SystemContext systemContext: WindowContextAware.context.systemContext
    readonly property var taxonomyManager: systemContext.taxonomyManager()

    readonly property var availableObjectTypes: [
        "nx.base.Face",
        "nx.base.Person",
        "nx.base.Unknown",
        "nx.base.Vehicle",
        "nx.base.Car",
        "nx.base.Bike",
    ]

    title: qsTr("Select Objects")
    minimumWidth: 500
    minimumHeight: 440
    maximumWidth: 500
    maximumHeight: 440

    function updateSelected()
    {
        let result = []
        for (let i = 0; i < objectTypes.count; ++i)
        {
            let item = objectTypes.itemAt(i)
            if (item.checked)
                result.push(item.id)
        }
        objectTypeIds = result
    }

    contentItem: ColumnLayout
    {
        spacing: 8

        CheckListItem
        {
            id: allObjects

            text: qsTr("All Objects")

            Layout.fillWidth: true
        }

        Panel
        {
            title: qsTr("Objects")
            enabled: !dialog.allObjects
            font.weight: Font.Medium

            Layout.fillWidth: true
            Layout.topMargin: 16

            contentItem: ColumnLayout
            {
                spacing: 8

                Repeater
                {
                    id: objectTypes

                    model: availableObjectTypes

                    CheckListItem
                    {
                        readonly property var id: modelData
                        readonly property var objectType: taxonomyManager.objectTypeById(id)

                        text: objectType ? objectType.name : ""
                        checked: dialog.objectTypeIds.includes(id)
                        iconSource:
                            objectType ? Analytics.IconManager.iconUrl(objectType.iconSource) : ""

                        Layout.fillWidth: true
                        Layout.leftMargin: objectType && objectType.baseType ? 20 : 0

                        onClicked: updateSelected()
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }

    WindowContextAware.onBeforeSystemChanged: reject()
}
