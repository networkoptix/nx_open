// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop 1.0

import Nx.Models 1.0

Dialog
{
    id: control

    title: qsTr("Select layout")

    property FilteredResourceProxyModel layoutModel: null

    property int selectedLayoutIndex: -1

    function reset()
    {
        searchEdit.text = ""
        selectedLayoutIndex = -1
    }

    function setSelectedLayoutIndex(index)
    {
        if (index < 0 || repeater.count <= index)
        {
            console.log("Warning: unexpected layoutIndex value: " + index)
            return
        }

        repeater.itemAt(index).checked = true
    }

    width: 300
    height: 400
    minimumWidth: 300
    minimumHeight: 400

    contentItem: Column
    {
        spacing: 16

        SearchField
        {
            id: searchEdit

            width: parent.width

            onTextChanged:
            {
                if (control.layoutModel)
                    control.layoutModel.setFilterRegularExpression(NxGlobals.escapeRegExp(text))
            }
        }

        Row
        {
            spacing: 10

            width: control.width
            height: parent.height - y

            Flickable
            {
                id: table

                width: control.contentItem.width -
                    (scrollBar.visible ? scrollBar.width + parent.spacing : 0)
                height: parent.height

                clip: true
                contentWidth: width
                contentHeight: radioButtonColumn.height

                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                interactive: true

                ScrollBar.vertical: scrollBar

                Column
                {
                    id: radioButtonColumn

                    spacing: 0

                    Repeater
                    {
                        id: repeater

                        model: control.layoutModel

                        delegate: Component
                        {
                            StyledRadioButton
                            {
                                width: table.width

                                iconSource:
                                    (model && model.iconKey && model.iconKey !== 0
                                        && ("image://resource/" + model.iconKey)) || ""

                                text: model.display

                                logicalId: model.logicalId

                                onCheckedChanged: control.selectedLayoutIndex = index
                            }
                        }
                    }
                }
            }

            ScrollBar
            {
                id: scrollBar

                anchors.top: table.top
                anchors.bottom: table.bottom

                policy: ScrollBar.AlwaysOn

                visible: table.height < table.contentHeight
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }
}
