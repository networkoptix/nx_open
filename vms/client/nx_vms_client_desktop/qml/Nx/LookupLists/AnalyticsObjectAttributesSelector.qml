// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

Control
{
    id: control

    enum SelectionState { Nothing, Partial, All }
    padding: 4

    signal selectionChanged

    property alias model: visualModel.model
    property var selectionState: AnalyticsObjectAttributesSelector.SelectionState.Partial

    contentItem: Column
    {
        spacing: 6

        Rectangle
        {
            width: parent.width
            height: selector.height + searchField.height + selector.anchors.topMargin + searchField.y
            color: ColorTheme.colors.dark6
            radius: 1
            border.color: ColorTheme.colors.dark5
            border.width: 1

            SearchField
            {
                id: searchField

                x: 4
                y: 4
                width: parent.width - x - selector.x

                onTextChanged: visualModel.updateVisibleItems()
            }

            Scrollable
            {
                id: selector

                x: 4
                width: parent.width - x
                height: 200
                anchors.top: searchField.bottom
                anchors.topMargin: 4
                scrollView.ScrollBar.vertical.width: 4

                contentItem: Column
                {
                    Repeater
                    {
                        model: visualModel
                    }
                }
            }
        }

        TextButton
        {
            text: qsTr("Select / Deselect All")
            onClicked:
            {
                selectionState = selectionState === AnalyticsObjectAttributesSelector.SelectionState.All
                    ? AnalyticsObjectAttributesSelector.SelectionState.Nothing
                    : AnalyticsObjectAttributesSelector.SelectionState.All
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
            checked: selectionState === AnalyticsObjectAttributesSelector.SelectionState.Partial
                ? model.checked
                : selectionState === AnalyticsObjectAttributesSelector.SelectionState.All

            onCheckedChanged:
            {
                if (selectionState !== AnalyticsObjectAttributesSelector.SelectionState.Partial)
                {
                    model.checked = checked
                    selectionChanged()
                }
            }

            onClicked:
            {
                model.checked = checked
                control.selectionState = AnalyticsObjectAttributesSelector.SelectionState.Partial
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
