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
    property var availableTypeIds: []

    readonly property SystemContext systemContext: WindowContextAware.context.systemContext
    readonly property var taxonomyManager: systemContext.taxonomyManager

    title: qsTr("Select Objects")
    minimumWidth: 500
    minimumHeight: 440
    maximumWidth: 500
    maximumHeight: 440
    modality: Qt.ApplicationModal

    function updateChildrenRecursively(parentIndex, checked) : int
    {
        const parentObjectType = taxonomyManager.objectTypeById(objectTypes.itemAt(parentIndex).id)
        for (let i = parentIndex + 1; i < objectTypes.count;)
        {
            const item = objectTypes.itemAt(i)
            const currentType = taxonomyManager.objectTypeById(item.id)

            if (currentType.baseType === parentObjectType)
            {
                item.checked = checked
                i = updateChildrenRecursively(i, checked)
            }
            else
            {
                return i;
            }
        }
        return objectTypes.count;
    }

    function updateSelected()
    {
        for (let i = 0; i < objectTypes.count;)
        {
            const item = objectTypes.itemAt(i)
            if (item.checked !== objectTypeIds.includes(item.id))
                i = updateChildrenRecursively(i, item.checked)
            else
                ++i
        }
    }

    function updateObjectTypeIds()
    {
        let result = []
        for (let i = 0; i < objectTypes.count; ++i)
        {
            const item = objectTypes.itemAt(i)
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
                id: scrollViewOwner

                spacing: 8
                width: parent.width

                ScrollView
                {
                    id: scrollView

                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    Layout.fillWidth: true
                    Layout.preferredHeight: 280

                    ColumnLayout
                    {
                        id: scrollViewChild

                        spacing: 8
                        width: scrollViewOwner.width - scrollView.effectiveScrollBarWidth

                        Repeater
                        {
                            id: objectTypes

                            model: availableTypeIds

                            CheckListItem
                            {
                                readonly property var id: modelData
                                readonly property var objectType: taxonomyManager.objectTypeById(id)

                                text: objectType ? objectType.name : ""
                                checked: dialog.objectTypeIds.includes(id)
                                iconSource:
                                    objectType ? Analytics.IconManager.iconUrl(objectType.iconSource) : ""

                                Layout.fillWidth: true
                                Layout.leftMargin: objectType ? objectType.baseTypeDepth * 20 : 0

                                onClicked:
                                {
                                    updateSelected()
                                    updateObjectTypeIds()
                                }
                            }
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
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }

    WindowContextAware.onBeforeSystemChanged: reject()
}
