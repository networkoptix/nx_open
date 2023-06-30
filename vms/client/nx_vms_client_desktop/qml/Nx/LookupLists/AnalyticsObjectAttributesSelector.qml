// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

Control
{
    id: control
    background: Rectangle { color: ColorTheme.colors.dark6 }
    padding: 4

    signal selectionChanged

    property alias model: visualModel.model

    contentItem: Column
    {
        spacing: 4

        SearchField
        {
            id: searchField
            width: parent.width
            onTextChanged: visualModel.updateVisibleItems()
        }

        Scrollable
        {
            height: 200
            width: parent.width

            contentItem: Column
            {
                Repeater { model: visualModel }
            }
        }
    }

    Component
    {
        id: checkBoxDelegate

        CheckBox
        {
            text: model.text
            font.pixelSize: 14
            checked: model.checked
            onClicked:
            {
                model.checked = checked
                selectionChanged()
            }
        }
    }

    DelegateModel
    {
        id: visualModel
        delegate: checkBoxDelegate
        filterOnGroup: "visible"

        groups: [
            DelegateModelGroup {
                name: "visible"
                includeByDefault: true
            }
        ]

        function updateVisibleItems()
        {
            const filter = searchField.text.toLowerCase()
            for (let i = 0; i < items.count; ++i)
            {
                const item = items.get(i)
                // Manage `visible` group membership.
                item.inVisible = !filter ||
                    item.model.text.toLowerCase().indexOf(filter) != -1
            }
        }
    }
}
